# tools/make_kbm_prompts.py <out dir>: the keyboard prompt pictures added for prompts/keyboard.txt (23 Nov), on top of
# make_kbm_keys.py's keys (our own art, no game assets):
#
#   tk_w / tk_a / tk_s / tk_d  the takedown diamond's discs with the key's letter instead of the pad's: same disc
#        colours and shape as the game's 50 px takedown buttons (the user wanted the takedowns to keep their colour
#        and shape), placed as on the pad: Y top (yellow) -> W, X left (blue) -> A, A bottom (green) -> S,
#        B right (red) -> D. Drawn with make_ps_buttons.py's disc (dark rim, highlight at the top, bevelled symbol
#        over a soft shadow); colours measured from the game's textures (texture dump, decoded 2026-11-23):
#        yellow disc (125,87,29), letter (205,143,51); blue (57,66,100) / (122,138,180); green (74,97,57) /
#        (133,168,112); red as the HUD glyph (115,40,41) / (205,101,98).
#   km_backspace  the menus' B (back) as a key, in the light grey menu style (Backspace goes back in menus).
#   k_space_sq    Space as a square key for the HUD font's square cells (Y: jump).
#   km_<key>      the tutorial boxes' and menus' whole 64x64 pictures in the light grey menu style (user, 23 Nov: all
#                 prompts light grey, no dark ones), with the same margin as km_enter so they sit at the size the
#                 game's 40 px discs did: E C R F G S Q Tab and the mouse's left / right buttons.
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_kbm_keys as kk  # noqa: E402
import make_ps_buttons as pb  # noqa: E402

LETTER_FONT = "/usr/share/fonts/truetype/google-fonts/ofl/montserrat/Montserrat[wght].ttf"

YELLOW = dict(body=(125, 87, 29), light=(164, 113, 41), sym_top=(236, 180, 86), sym_mid=(205, 143, 51),
              sym_bot=(158, 104, 34))


def letter_mask(letter, k):
    """A heavy capital centred on the disc, as tall as the game's letters (31 texels at k 1.6), at most 42 wide."""
    def mask(kind, dx=0.0, dy=0.0, k=k):
        m = Image.new("L", (pb.N * pb.S, pb.N * pb.S), 0)
        g = ImageDraw.Draw(m)
        size = 60 * pb.S
        while True:
            f = ImageFont.truetype(LETTER_FONT, size)
            f.set_variation_by_name("Black")
            b = g.textbbox((0, 0), letter, font=f)
            if (b[3] - b[1]) <= 31 * k / 1.6 * pb.S and (b[2] - b[0]) <= 42 * pb.S:
                break
            size -= pb.S // 2
        x = (pb.C + dx) * pb.S - (b[2] - b[0]) / 2 - b[0]
        y = (pb.C + dy) * pb.S - (b[3] - b[1]) / 2 - b[1]
        g.text((x, y), letter, font=f, fill=255)
        return m
    return mask


def takedown_disc(letter, palette, out):
    saved = pb.symbol_mask
    pb.symbol_mask = letter_mask(letter, 1.6)
    try:
        pb.button(letter, **palette, outer_shadow=False, rim=(12, 10, 10), k=1.6, r=24.5, out=out)
    finally:
        pb.symbol_mask = saved


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "prompts/kbm")
    out.mkdir(parents=True, exist_ok=True)
    takedown_disc("W", YELLOW, str(out / "tk_w.png"))
    takedown_disc("A", pb.G_BLUE, str(out / "tk_a.png"))
    takedown_disc("S", pb.G_GREEN, str(out / "tk_s.png"))
    takedown_disc("D", pb.G_RED, str(out / "tk_d.png"))
    kk.key("←", str(out / "km_backspace.png"), kk.MENU, width=96, px=60)
    kk.key("SPACE", str(out / "k_space_sq.png"), kk.HUD)
    for t in ("E", "C", "R", "F", "G", "S", "Q"):
        kk.key(t, str(out / f"km_{t.lower()}.png"), kk.MENU)
    kk.key("TAB", str(out / "km_tab.png"), kk.MENU)  # square: "replace" stretches to the 64x64 texture
    for m in ("left", "right"):
        kk.mouse(m, str(out / f"km_mouse_{m}.png"), kk.MENU)


if __name__ == "__main__":
    main()
