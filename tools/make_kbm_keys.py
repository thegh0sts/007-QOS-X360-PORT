# tools/make_kbm_keys.py <out dir>: draws keyboard and mouse prompt pictures for 007 (prompts/kbm/*.png, our own art,
# no game assets, nothing taken from the Bourne port), in the three styles the game's own button pictures use, so a
# keyboard prompt sits where an Xbox one did without looking out of place:
#
#   k_*   HUD font glyphs (the 28 and 14 px cells: "Dash - Hold [A]"): light grey cap (216,218,216) with a mid-grey edge
#         (150,150,150) and the label in dark grey (131,129,131), as the game's LB/RB glyphs; drawn 64 px high.
#   kt_*  tutorial box and takedown pictures (64x64): dark grey cap (88,89,88), black edge (30,32,30), light grey label
#         (197,198,197) over a soft shadow, as the game's LB/LT tutorial pictures.
#   km_*  menu buttons (64x64, Scaleform btn_accept/btn_back): Enter and Esc in the light grey style with a soft
#         shadow (the user didn't want the menu discs' green/maroon on keys, 21 Nov); kt_enter/kt_esc are the dark ones.
#
# Each key is a cap seen from the front: a rounded block, its top face a little smaller and lighter than the sides
# (the 360 glyphs' soft top highlight), the label bevelled like the game's (lighter top, darker bottom, soft shadow
# below). Wide keys (Space, Tab, Enter, Shift) are wider pictures; "fit" in playstation.txt keeps their shape.
# Mouse pictures: a mouse seen from above with the used button lit (left: fire, right: aim) or arrows (look).
#
# Keys drawn: the user's 007 layout (handoff, 21 Nov): W A S D (and WASD), Space, E, R, G, 3, 4, X, Q, Tab, plus
# Enter, Esc, Shift, F, C, V, 1, 2 for menus and spares; mouse left / right / middle / move.
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 8  # drawn at 8x, scaled down
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

HUD = dict(edge=(150, 150, 150), side=(184, 186, 184), top=(216, 218, 216), shine=(240, 241, 240),
           lab_top=(150, 148, 150), lab=(131, 129, 131), lab_bot=(104, 102, 104), lab_shadow=None, shadow=False)
TUT = dict(edge=(30, 32, 30), side=(66, 67, 66), top=(88, 89, 88), shine=(130, 130, 130),
           lab_top=(222, 223, 222), lab=(197, 198, 197), lab_bot=(150, 151, 150), lab_shadow=(25, 25, 25), shadow=True)
MENU = dict(HUD, shadow=True)

def font(px):
    return ImageFont.truetype(FONT, int(px * S))


def label_layer(size, text, cx, cy, px, st):
    """The label, bevelled: light top, mid body, darker bottom, with a soft shadow below if the style has one."""
    w, h = size
    mask = Image.new("L", (w * S, h * S), 0)
    g = ImageDraw.Draw(mask)
    f = font(px)
    b = g.textbbox((0, 0), text, font=f)
    x = cx * S - (b[2] - b[0]) / 2 - b[0]
    y = cy * S - (b[3] - b[1]) / 2 - b[1]
    g.text((x, y), text, font=f, fill=255)
    out = Image.new("RGBA", mask.size, (0, 0, 0, 0))
    if st["lab_shadow"]:
        sh = Image.new("RGBA", mask.size, st["lab_shadow"] + (255,))
        sh.putalpha(mask.filter(ImageFilter.GaussianBlur(S * 0.8)).transform(
            mask.size, Image.AFFINE, (1, 0, -0.6 * S, 0, 1, -1.4 * S)))
        out.alpha_composite(sh)
    grad = Image.new("RGBA", mask.size)
    gd = ImageDraw.Draw(grad)
    top, bot = b[1] + y, b[3] + y
    for yy in range(mask.size[1]):
        t = min(1.0, max(0.0, (yy - top) / max(1, bot - top)))
        gd.line((0, yy, mask.size[0], yy), fill=(st["lab_top"] if t < 0.22 else st["lab"] if t < 0.78 else
                                                 st["lab_bot"]) + (255,))
    grad.putalpha(mask)
    out.alpha_composite(grad)
    return out


def cap(size, box, st, radius):
    """A key cap in `box` (x0, y0, x1, y1, texels): edge, darker sides, lighter top face with a soft shine."""
    w, h = size
    x0, y0, x1, y1 = box
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    if st["shadow"]:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle(((x0 + 1) * S, (y0 + 2) * S, (x1 + 1) * S, (y1 + 2) * S),
                                             radius=radius * S, fill=(0, 0, 0, 150))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 2)))
    g = ImageDraw.Draw(img)
    g.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=radius * S, fill=st["edge"] + (255,))
    g.rounded_rectangle(((x0 + 2) * S, (y0 + 2) * S, (x1 - 2) * S, (y1 - 2) * S), radius=(radius - 2) * S,
                        fill=st["side"] + (255,))
    # the top face: inset more at the bottom (the cap's front lip)
    face = ((x0 + 4) * S, (y0 + 3) * S, (x1 - 4) * S, (y1 - 6) * S)
    g.rounded_rectangle(face, radius=(radius - 3) * S, fill=st["top"] + (255,))
    shine = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shine).rounded_rectangle((face[0] + 2 * S, face[1] + S, face[2] - 2 * S,
                                             face[1] + (face[3] - face[1]) * 0.45), radius=(radius - 4) * S,
                                            fill=st["shine"] + (70,))
    img.alpha_composite(shine.filter(ImageFilter.GaussianBlur(S * 2.5)))
    return img, ((face[0] + face[2]) / 2 / S, (face[1] + face[3]) / 2 / S)


def key(text, out, st, width=64, height=64, px=None):
    """One key: square for a single character, wider for words (Space, Tab...)."""
    m = 6 if st["shadow"] else 2  # room for the shadow
    box = (m, m + 2, width - m, height - m)
    img, (cx, cy) = cap((width, height), box, st, radius=11)
    if px is None:
        px = 40 if len(text) == 1 else 24 if len(text) <= 3 else 18
    # shrink a label that doesn't fit the top face (ESC on a square key)
    face_w = (box[2] - box[0]) - 14
    probe = ImageDraw.Draw(Image.new("L", (1, 1)))
    while px > 10:
        b = probe.textbbox((0, 0), text, font=font(px))
        if (b[2] - b[0]) / S <= face_w:
            break
        px -= 1
    img.alpha_composite(label_layer((width, height), text, cx, cy, px, st))
    img.resize((width, height), Image.LANCZOS).save(out)
    print(out)


def wasd(out, st):
    """W over A S D, one picture (movement)."""
    w, h = 96, 64
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    k = 30
    spots = {"W": (33, 1), "A": (2, 33), "S": (33, 33), "D": (64, 33)}
    for t, (x, y) in spots.items():
        c, (cx, cy) = cap((w, h), (x, y, x + k, y + k - 1), st, radius=6)
        img.alpha_composite(c)
        img.alpha_composite(label_layer((w, h), t, cx, cy, 17, st))
    img.resize((w, h), Image.LANCZOS).save(out)
    print(out)


def mouse(which, out, st):
    """A mouse from above: body in the style's cap colours, the used button lit (left, right, middle) or arrows."""
    n = 64
    img = Image.new("RGBA", (n * S, n * S), (0, 0, 0, 0))
    bx0, by0, bx1, by1 = 12, 4, 52, 60
    if st["shadow"]:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).rounded_rectangle(((bx0 + 1) * S, (by0 + 2) * S, (bx1 + 1) * S, (by1 + 2) * S),
                                             radius=18 * S, fill=(0, 0, 0, 150))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 2)))
    g = ImageDraw.Draw(img)
    g.rounded_rectangle((bx0 * S, by0 * S, bx1 * S, by1 * S), radius=18 * S, fill=st["edge"] + (255,))
    g.rounded_rectangle(((bx0 + 2) * S, (by0 + 2) * S, (bx1 - 2) * S, (by1 - 2) * S), radius=16 * S,
                        fill=st["side"] + (255,))
    split = 29  # where the buttons end
    lit = st["lab"]
    for side, (x0, x1) in (("left", (bx0 + 3, 31.2)), ("right", (32.8, bx1 - 3))):
        fill = lit if which == side else st["top"]
        g.rounded_rectangle((x0 * S, (by0 + 3) * S, x1 * S, split * S), radius=11 * S, fill=fill + (255,))
    g.rectangle(((bx0 + 3) * S, (split - 6) * S, (bx1 - 3) * S, split * S), fill=st["side"] + (255,))
    for side, (x0, x1) in (("left", (bx0 + 3, 31.2)), ("right", (32.8, bx1 - 3))):  # square off the buttons' bottoms
        fill = lit if which == side else st["top"]
        g.rectangle((x0 * S, (by0 + 10) * S, x1 * S, (split - 1) * S), fill=fill + (255,))
    wheel = lit if which == "middle" else st["edge"]
    g.rounded_rectangle((29.8 * S, 11 * S, 34.2 * S, 22 * S), radius=2 * S, fill=wheel + (255,))
    if which == "move":  # four arrows round the lower body
        a = st["lab"]
        cx, cy = 32, 44
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            tip = (cx + dx * 11, cy + dy * 11)
            base = (cx + dx * 5, cy + dy * 5)
            px, py = -dy, dx
            g.polygon([(tip[0] * S, tip[1] * S), ((base[0] + px * 4.5) * S, (base[1] + py * 4.5) * S),
                       ((base[0] - px * 4.5) * S, (base[1] - py * 4.5) * S)], fill=a + (255,))
    shine = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shine).rounded_rectangle(((bx0 + 6) * S, (by0 + 4) * S, (bx1 - 6) * S, 14 * S), radius=8 * S,
                                            fill=st["shine"] + (60,))
    img.alpha_composite(shine.filter(ImageFilter.GaussianBlur(S * 2)))
    img.resize((n, n), Image.LANCZOS).save(out)
    print(out)


KEYS = ["W", "A", "S", "D", "E", "R", "G", "Q", "X", "F", "C", "V", "1", "2", "3", "4"]
WIDE = {"SPACE": 112, "TAB": 80, "ENTER": 96, "SHIFT": 96, "ESC": 64}


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "prompts/kbm")
    out.mkdir(parents=True, exist_ok=True)
    for prefix, st in (("k", HUD), ("kt", TUT)):
        for t in KEYS:
            key(t, str(out / f"{prefix}_{t.lower()}.png"), st)
        for t, w in WIDE.items():
            key(t, str(out / f"{prefix}_{t.lower()}.png"), st, width=w)
        wasd(str(out / f"{prefix}_wasd.png"), st)
        for m in ("left", "right", "middle", "move"):
            mouse(m, str(out / f"{prefix}_mouse_{m}.png"), st)
    key("ENTER", str(out / "km_enter.png"), MENU, width=96)
    key("ESC", str(out / "km_esc.png"), MENU)


# Isometric versions (`*_iso`, an alternative look, 21 Nov): the cap seen from above and to the front-left, so its top
# face is a sheared, slightly squashed panel with the label on it, and the front and right sides show as darker
# bands. Same colours as the flat styles; same picture sizes, so either set drops into the same prompt slots.
SHEAR = 0.32   # top face: x shifts by this much per unit of height (leans right going up)
SQUASH = 0.78  # top face height kept
DEPTH = 7.0    # side depth, texels


def _top_face(fw, fh, text, px, st, radius):
    """The flat top face with its label, fw x fh texels at 8x."""
    face = Image.new("RGBA", (int(fw * S), int(fh * S)), (0, 0, 0, 0))
    g = ImageDraw.Draw(face)
    g.rounded_rectangle((0, 0, fw * S - 1, fh * S - 1), radius=radius * S, fill=st["edge"] + (255,))
    g.rounded_rectangle((1.5 * S, 1.5 * S, fw * S - 1.5 * S, fh * S - 1.5 * S), radius=(radius - 1.5) * S,
                        fill=st["top"] + (255,))
    shine = Image.new("RGBA", face.size, (0, 0, 0, 0))
    ImageDraw.Draw(shine).rounded_rectangle((3 * S, 2.5 * S, fw * S - 3 * S, fh * S * 0.45), radius=(radius - 3) * S,
                                            fill=st["shine"] + (70,))
    face.alpha_composite(shine.filter(ImageFilter.GaussianBlur(S * 2.5)))
    if text:
        probe = ImageDraw.Draw(Image.new("L", (1, 1)))
        while px > 10:
            b = probe.textbbox((0, 0), text, font=font(px))
            if (b[2] - b[0]) / S <= fw - 12:
                break
            px -= 1
        face.alpha_composite(label_layer((int(fw), int(fh)), text, fw / 2, fh / 2, px, st).crop(
            (0, 0, face.size[0], face.size[1])))
    return face


def _shear_up(img, h_px):
    """Squash vertically and lean the top to the right (an oblique/isometric view of a flat panel)."""
    w0, h0 = img.size
    nh = int(h0 * SQUASH)
    sq = img.resize((w0, nh), Image.LANCZOS)
    extra = int(nh * SHEAR)
    out = Image.new("RGBA", (w0 + extra, nh), (0, 0, 0, 0))
    # x_src = x_dst - SHEAR * (nh - y_dst)
    return sq.transform(out.size, Image.AFFINE, (1, SHEAR, -SHEAR * nh, 0, 1, 0), resample=Image.BICUBIC)


def iso_key(text, out, st, width=64, height=64, px=None):
    if px is None:
        px = 40 if len(text) == 1 else 24 if len(text) <= 3 else 18
    m = 5 if st["shadow"] else 2
    fh = (height - 2 * m - DEPTH) / SQUASH
    fw = width - 2 * m - DEPTH - fh * SQUASH * SHEAR
    radius = 9
    face = _shear_up(_top_face(fw, fh, text, px, st, radius), fh)
    img = Image.new("RGBA", (width * S, height * S), (0, 0, 0, 0))
    fx, fy = m * S, m * S
    # body: the face's outline pushed down-right DEPTH texels, darker towards the front
    alpha = face.split()[3]
    body = Image.new("RGBA", img.size, (0, 0, 0, 0))
    steps = int(DEPTH * 2)
    for i in range(steps, 0, -1):
        t = i / steps
        dx, dy = int(fx + DEPTH * S * t * 0.55), int(fy + DEPTH * S * t)
        c = tuple(int(a + (b - a) * t) for a, b in zip(st["side"], st["edge"]))
        layer = Image.new("RGBA", face.size, c + (255,))
        layer.putalpha(alpha)
        body.alpha_composite(layer, (dx, dy))
    if st["shadow"]:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        sl = Image.new("RGBA", face.size, (0, 0, 0, 150))
        sl.putalpha(alpha.point(lambda v: v * 150 // 255))
        sh.alpha_composite(sl, (int(fx + DEPTH * S * 0.9), int(fy + DEPTH * S * 1.4)))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 2)))
    img.alpha_composite(body)
    img.alpha_composite(face, (fx, fy))
    img.resize((width, height), Image.LANCZOS).save(out)
    print(out)


def iso_mouse(which, out, st):
    """The mouse pictures already show the mouse from above; the isometric set leans them like the keys."""
    flat = Image.new("RGBA", (64, 64))
    tmp = out + ".flat.png"
    mouse(which, tmp, st)
    flat = Image.open(tmp).convert("RGBA")
    Path(tmp).unlink()
    flat = flat.crop(flat.getbbox())  # the mouse without its margin, so it fills the picture like the keys
    big = flat.resize((flat.width * S, flat.height * S), Image.LANCZOS)
    leaned = _shear_up(big, 64)
    canvas = Image.new("RGBA", (64 * S, 64 * S), (0, 0, 0, 0))
    lw, lh = leaned.size
    scale = min(60 * S / lw, 60 * S / lh)
    leaned = leaned.resize((int(lw * scale), int(lh * scale)), Image.LANCZOS)
    canvas.alpha_composite(leaned, ((64 * S - leaned.size[0]) // 2, (64 * S - leaned.size[1]) // 2))
    canvas.resize((64, 64), Image.LANCZOS).save(out)
    print(out)


def main_iso(out):
    for prefix, st in (("k", HUD), ("kt", TUT)):
        for t in KEYS:
            iso_key(t, str(out / f"{prefix}_iso_{t.lower()}.png"), st)
        for t, w in WIDE.items():
            iso_key(t, str(out / f"{prefix}_iso_{t.lower()}.png"), st, width=w)
        for m in ("left", "right", "middle", "move"):
            iso_mouse(m, str(out / f"{prefix}_iso_mouse_{m}.png"), st)
    iso_key("ENTER", str(out / "km_iso_enter.png"), MENU, width=96)
    iso_key("ESC", str(out / "km_iso_esc.png"), MENU)


# Tilted-back versions (`*_tilt_*`, 21 Nov, the user's pick of the 3D looks: "take the top view and tilt back"): the
# flat key's top face leans away from the viewer, so it narrows towards the top (perspective) and is a little
# shorter, and the cap's front edge shows below it as a darker band. No sideways lean; the label stays upright.
TILT_TOP = 0.84    # top edge width, as a share of the bottom edge
TILT_SQUASH = 0.80 # face height kept
TILT_DEPTH = 6.0   # front edge, texels


def _perspective_coeffs(dst, src):
    """PIL PERSPECTIVE coefficients mapping output points dst[i] to input points src[i] (4 pairs)."""
    import numpy as np
    a = []
    for (x, y), (u, v) in zip(dst, src):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
    b = [c for pt in src for c in pt]
    return np.linalg.solve(np.array(a, float), np.array(b, float)).tolist()


def _tilt_back(face):
    """The face as a trapezoid: full width at the bottom, TILT_TOP at the top, TILT_SQUASH as tall."""
    w, h = face.size
    nh = int(h * TILT_SQUASH)
    inset = w * (1 - TILT_TOP) / 2
    dst = [(inset, 0), (w - inset, 0), (w, nh), (0, nh)]
    src = [(0, 0), (w, 0), (w, h), (0, h)]
    return face.transform((w, nh), Image.PERSPECTIVE, _perspective_coeffs(dst, src), resample=Image.BICUBIC)


def _darker(st, k):
    return {n: (tuple(int(c * k) for c in v) if isinstance(v, tuple) else v) for n, v in st.items()}


def tilt_key(text, out, st, width=64, height=64, px=None, pressed=False):
    """pressed: the cap pushed down (for animated prompts such as the takedown diamond): the face sits lower by most of
    the front edge, which shrinks to a sliver, the face is a little darker, and the shadow tightens."""
    depth = TILT_DEPTH
    drop = 0.0
    if pressed:
        drop = TILT_DEPTH - 1.5
        depth = 1.5
        st = _darker(st, 0.86)
    if px is None:
        px = 40 if len(text) == 1 else 24 if len(text) <= 3 else 18
    m = 5 if st["shadow"] else 2
    fw = width - 2 * m
    fh = (height - 2 * m - TILT_DEPTH) / TILT_SQUASH
    face = _tilt_back(_top_face(fw, fh, text, px, st, 9))
    img = Image.new("RGBA", (width * S, height * S), (0, 0, 0, 0))
    fx, fy = m * S, int((m + drop) * S)
    alpha = face.split()[3]
    if st["shadow"]:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        sl = Image.new("RGBA", face.size, (0, 0, 0, 255))
        sl.putalpha(alpha.point(lambda v: v * 150 // 255))
        sh.alpha_composite(sl, (fx, int(fy + depth * S + (1 if pressed else 2) * S)))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * (1.2 if pressed else 2))))
    steps = max(2, int(depth * 2))
    for i in range(steps, 0, -1):  # the front edge: the face's outline pushed straight down, darker at the bottom
        t = i / steps
        c = tuple(int(a + (b - a) * t) for a, b in zip(st["side"], st["edge"]))
        layer = Image.new("RGBA", face.size, c + (255,))
        layer.putalpha(alpha)
        img.alpha_composite(layer, (fx, int(fy + depth * S * t)))
    img.alpha_composite(face, (fx, fy))
    img.resize((width, height), Image.LANCZOS).save(out)
    print(out)


def tilt_mouse(which, out, st):
    tmp = out + ".flat.png"
    mouse(which, tmp, st)
    flat = Image.open(tmp).convert("RGBA")
    Path(tmp).unlink()
    flat = flat.crop(flat.getbbox())
    big = flat.resize((flat.width * S, flat.height * S), Image.LANCZOS)
    tilted = _tilt_back(big)
    canvas = Image.new("RGBA", (64 * S, 64 * S), (0, 0, 0, 0))
    tw, th = tilted.size
    scale = min(60 * S / tw, 60 * S / th)
    tilted = tilted.resize((int(tw * scale), int(th * scale)), Image.LANCZOS)
    canvas.alpha_composite(tilted, ((64 * S - tilted.size[0]) // 2, (64 * S - tilted.size[1]) // 2))
    canvas.resize((64, 64), Image.LANCZOS).save(out)
    print(out)


def main_tilt(out):
    for prefix, st in (("k", HUD), ("kt", TUT)):
        for t in KEYS:
            tilt_key(t, str(out / f"{prefix}_tilt_{t.lower()}.png"), st)
        for t, w in WIDE.items():
            tilt_key(t, str(out / f"{prefix}_tilt_{t.lower()}.png"), st, width=w)
        for m in ("left", "right", "middle", "move"):
            tilt_mouse(m, str(out / f"{prefix}_tilt_mouse_{m}.png"), st)
        # pressed frames for animated prompts (the takedown diamond): every single-character key
        for t in KEYS:
            tilt_key(t, str(out / f"{prefix}_tilt_{t.lower()}_pressed.png"), st, pressed=True)
    tilt_key("ENTER", str(out / "km_tilt_enter.png"), MENU, width=96)
    tilt_key("ESC", str(out / "km_tilt_esc.png"), MENU)


if __name__ == "__main__":
    main()
    main_iso(Path(sys.argv[1] if len(sys.argv) > 1 else "prompts/kbm"))
    main_tilt(Path(sys.argv[1] if len(sys.argv) > 1 else "prompts/kbm"))
