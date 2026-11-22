"""007: Quantum of Solace - pipeline masters without shader microcode, from the game's fastfiles.

    python tools/ff_shader_refs.py index MANIFEST OUT.refs --game game [--keep OLD.refs] [--cover]
    python tools/ff_shader_refs.py check IN.refs --game game

MANIFEST is the native renderer's pipeline manifest (Documents/qos/cache/native/415607FF.manifest.bin, or a master
merged from several). OUT.refs names each of its shaders by where the game's own files hold it, so the file carries no
shader microcode and can ship with the port (pipelines/ next to qos.exe). The renderer (native patch 0027) reads the
shaders back from the player's fastfiles at its first start, checks each against its hash, and keeps them in
415607FF.master.bin in its cache folder, so later starts read nothing.

Refs format (the skullburglar renderer's, tools/shader_refs.py there):
  header  8-byte magic 'BOTSPR01', then the manifest's 16-byte header
  'R'     u64 shader hash, u32 type, u32 dword count, u16 source length, source (UTF-8), u32 byte offset,
          u16 patch count, patch count x (u16 dword index, u32 dword)
  'P'     one pipeline record, as in the manifest

Sources are fastfiles under the game root (`english/common.ff`, `whites_estate.ff`); offsets are into the
decompressed zone. 007's fastfile: big-endian header of 7 dwords (0x1D6, the zone's size, ...), then one zlib stream.
The renderer tries other language folders when the named one is missing, so `english/` refs work for every language.
Vertex shaders are stored as compiled; the XDK rewrites their vfetch block from the bound vertex declaration when they
are used, so 2-60 dwords of a vertex shader differ (the patches). Shaders found nowhere are left out, with their
pipelines kept (the renderer drops pipelines whose shaders it cannot rebuild).

Needs numpy (pip install numpy).
"""
from __future__ import annotations

import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

import numpy as np

MAGIC = b"BOTSPR01"
MASTER_HEADER = 16
KEY = 8  # bytes of a shader's start used to find candidates (the control-flow header, never patched)


def zone(path: Path) -> bytes:
    data = path.read_bytes()
    version, size = struct.unpack_from(">II", data, 0)
    z = zlib.decompressobj()
    out = z.decompress(data[28:])
    if not z.eof or len(out) != size:
        raise SystemExit(f"{path}: not a fastfile this tool knows (version {version:#x}, size {size}, got {len(out)})")
    return out


def read_master(path):
    data = Path(path).read_bytes()
    if not data.startswith(b"BOTSPM"):
        raise SystemExit(f"{path}: not a pipeline manifest")
    record_size = struct.unpack_from("<I", data, 8)[0]
    shaders, pipelines, pos = {}, [], MASTER_HEADER
    while pos < len(data):
        tag = data[pos:pos + 1]
        if tag == b"S" and pos + 17 <= len(data):
            hash_, type_, dwords = struct.unpack_from("<QII", data, pos + 1)
            end = pos + 17 + dwords * 4
            if end > len(data):
                break
            shaders.setdefault(hash_, (type_, data[pos + 17:end]))
            pos = end
        elif tag == b"P" and pos + 1 + record_size <= len(data):
            pipelines.append(data[pos:pos + 1 + record_size])
            pos += 1 + record_size
        else:
            break
    return data[:MASTER_HEADER], shaders, pipelines


def sources(game: Path):
    """Fastfiles in search order: the always-loaded ones first (english, else any language), then the levels."""
    names = []
    for lang in ["english"] + sorted(p.name for p in game.iterdir() if p.is_dir() and p.name != "english"):
        found = [f"{lang}/{n}" for n in ("code_post_gfx.ff", "common.ff", "ui.ff") if (game / lang / n).exists()]
        if found:
            names += found
            break
    levels = sorted(p.name for p in game.glob("*.ff"))
    names += [n for n in levels if not n.startswith(("mp_", "unlock_"))]
    names += [n for n in levels if n.startswith("unlock_")]
    names += [n for n in levels if n.startswith("mp_")]
    return names


def search(data: bytes, todo: dict, found: dict, name: str):
    """Updates found[hash] = (name, offset, patches) for the shaders in todo {hash: code}: exact matches, else the
    same-length blob starting with the same KEY bytes with the fewest differing dwords (at most a third)."""
    by_key = {}
    for h, code in todo.items():
        by_key.setdefault(code[:KEY], []).append(h)
    keys = np.array(sorted(int.from_bytes(k, "big") for k in by_key), dtype=np.uint64)
    for phase in range(8):
        count = (len(data) - phase) // 8
        if count <= 0:
            continue
        words = np.frombuffer(data, dtype=">u8", count=count, offset=phase).astype(np.uint64)
        hits = np.nonzero(np.isin(words, keys))[0]
        for i in hits:
            at = phase + int(i) * 8
            for h in by_key[data[at:at + KEY]]:
                code = todo[h]
                if at + len(code) > len(data):
                    continue
                blob = data[at:at + len(code)]
                if blob == code:
                    found[h] = (name, at, [])
                    continue
                n = len(code) // 4
                diff = [j for j in range(n) if blob[j * 4:j * 4 + 4] != code[j * 4:j * 4 + 4]]
                if len(diff) <= n // 3 and (h not in found or len(diff) < len(found[h][2])):
                    found[h] = (name, at, [(j, code[j * 4:j * 4 + 4]) for j in diff])


def cover(game: Path, shaders: dict) -> dict:
    """--cover: every source searched for every shader, then the sources picked greedily by shaders newly covered per
    MB of zone (the renderer inflates each named fastfile whole), so a first start reads as little as it can. Within a
    picked source each shader keeps its best match (exact, else fewest patched dwords)."""
    todo = {h: code for h, (_, code) in shaders.items()}
    per_source, sizes = {}, {}
    for name in sources(game):
        start = time.time()
        data = zone(game / name)
        sizes[name] = len(data)
        hits = {}
        search(data, todo, hits, name)
        per_source[name] = hits
        print(f"  {name}: {len(hits)} shaders ({len(data) / 1048576:.0f} MB zone, {time.time() - start:.1f} s)",
              flush=True)
    found, left = {}, set().union(*(set(h) for h in per_source.values())) if per_source else set()
    while left:
        name = max(per_source, key=lambda n: len(left & per_source[n].keys()) / max(sizes[n], 1))
        gain = left & per_source[name].keys()
        if not gain:
            break
        for h in gain:
            found[h] = per_source[name][h]
        left -= gain
        print(f"  picked {name}: {len(gain)} shaders ({sizes[name] / 1048576:.0f} MB)")
    picked = sorted({v[0] for v in found.values()})
    print(f"  {len(picked)} sources, {sum(sizes[n] for n in picked) / 1048576:.0f} MB of zones to inflate")
    return found


def index(args):
    header, shaders, pipelines = read_master(args.manifest)
    game = Path(args.game)
    found = cover(game, shaders) if args.cover else {}
    for name in ([] if args.cover else sources(game)):
        todo = {h: code for h, (_, code) in shaders.items() if h not in found or found[h][2]}
        if not todo:
            break
        start = time.time()
        before = sum(1 for v in found.values() if not v[2])
        search(zone(game / name), todo, found, name)
        exact = sum(1 for v in found.values() if not v[2])
        print(f"  {name}: {exact - before} more exact, {len(found)}/{len(shaders)} found ({time.time() - start:.1f} s)",
              flush=True)
    # --keep: an earlier refs file's shaders and pipelines that this manifest doesn't have stay in (a manifest from a
    # fresh cache holds only what that run met).
    old_refs, old_pipelines = [], []
    if args.keep:
        old_header, old_refs, old_pipelines = read_refs(args.keep)
        if old_header[8:12] != header[8:12]:
            raise SystemExit(f"{args.keep}: pipeline records of another size than the manifest's")
    with open(args.out, "wb") as f:
        f.write(MAGIC + header)
        kept, written = 0, set()
        for h, (type_, code) in shaders.items():
            if h not in found:
                continue
            name, offset, patches = found[h]
            src = name.encode()
            f.write(b"R" + struct.pack("<QIIH", h, type_, len(code) // 4, len(src)) + src)
            f.write(struct.pack("<IH", offset, len(patches)))
            for j, dword in patches:
                f.write(struct.pack("<H", j) + dword)
            kept += 1
            written.add(h)
        carried = 0
        for h, type_, dwords, name, offset, patches in old_refs:
            if h in written:
                continue
            src = name.encode()
            f.write(b"R" + struct.pack("<QIIH", h, type_, dwords, len(src)) + src)
            f.write(struct.pack("<IH", offset, len(patches)))
            for j, dword in patches:
                f.write(struct.pack("<H", j) + dword)
            carried += 1
            written.add(h)
        seen = set(pipelines)
        extra = [r for r in old_pipelines if r not in seen and not seen.add(r)]
        for record in pipelines + extra:
            f.write(record)
    if args.keep:
        print(f"kept from {args.keep}: {carried} shaders, {len(extra)} pipelines")
    near = sum(1 for v in found.values() if v[2])
    missing = [f"{h:016X}" for h in shaders if h not in found]
    print(f"{args.out}: {kept}/{len(shaders)} shaders referenced ({kept - near} exact, {near} patched), "
          f"{len(pipelines)} pipelines")
    if missing:
        print(f"not found ({len(missing)}):", " ".join(missing))


def read_refs(path):
    data = Path(path).read_bytes()
    if not data.startswith(MAGIC):
        raise SystemExit(f"{path}: not a refs file")
    header = data[len(MAGIC):len(MAGIC) + MASTER_HEADER]
    record_size = struct.unpack_from("<I", header, 8)[0]
    refs, pipelines, pos = [], [], len(MAGIC) + MASTER_HEADER
    while pos < len(data):
        tag = data[pos:pos + 1]
        if tag == b"R":
            h, type_, dwords, n = struct.unpack_from("<QIIH", data, pos + 1)
            pos += 19
            name = data[pos:pos + n].decode()
            offset, count = struct.unpack_from("<IH", data, pos + n)
            pos += n + 6
            patches = [(struct.unpack_from("<H", data, pos + i * 6)[0], data[pos + i * 6 + 2:pos + i * 6 + 6])
                       for i in range(count)]
            pos += count * 6
            refs.append((h, type_, dwords, name, offset, patches))
        elif tag == b"P":
            pipelines.append(data[pos:pos + 1 + record_size])
            pos += 1 + record_size
        else:
            raise SystemExit(f"{path}: bad chunk at {pos:#x}")
    return header, refs, pipelines


def check(args):
    """Rebuilds every shader from the fastfiles and compares it with a manifest's copy (--manifest) if given."""
    _, refs, pipelines = read_refs(args.refs)
    known = read_master(args.manifest)[1] if args.manifest else {}
    game, cache, ok, bad = Path(args.game), {}, 0, []
    for h, type_, dwords, name, offset, patches in sorted(refs, key=lambda r: r[3]):
        if name not in cache:
            cache.clear()
            cache[name] = zone(game / name)
        code = bytearray(cache[name][offset:offset + dwords * 4])
        for j, dword in patches:
            code[j * 4:j * 4 + 4] = dword
        if len(code) != dwords * 4 or (h in known and bytes(code) != known[h][1]):
            bad.append(f"{h:016X}")
        else:
            ok += 1
    print(f"{args.refs}: {ok}/{len(refs)} shaders rebuilt{' and equal to the manifest' if known else ''}, "
          f"{len(pipelines)} pipelines")
    if bad:
        print("bad:", " ".join(bad))
        sys.exit(1)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    i = sub.add_parser("index")
    i.add_argument("manifest")
    i.add_argument("out")
    i.add_argument("--game", default="game")
    i.add_argument("--keep", help="an earlier refs file whose shaders and pipelines missing here are kept")
    i.add_argument("--cover", action="store_true",
                   help="search every fastfile and pick the fewest MB that hold all the shaders (slower to index, "
                        "faster first start)")
    c = sub.add_parser("check")
    c.add_argument("refs")
    c.add_argument("--game", default="game")
    c.add_argument("--manifest")
    args = p.parse_args()
    {"index": index, "check": check}[args.cmd](args)


if __name__ == "__main__":
    main()
