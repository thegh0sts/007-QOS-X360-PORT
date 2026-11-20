# tools/make_ps_buttons.py <out dir>: draws the PlayStation button pictures that replace the game's Xbox ones
# (prompts/*.png, our own art, no game assets) in the style of the game's 64x64 menu buttons (btn_accept, btn_back):
# a disc 40 px across in the middle of the texture, flat colour with a soft highlight at the top, and the symbol
# bevelled (light top, darker bottom) over a soft dark shadow. Colours measured from the game's textures (texture
# dump, 2026-11-20): A green disc (74,97,57), letter (133,168,112); B maroon disc (84,41,41), letter (189,118,117)
# with a soft black shadow round the disc. Cross takes A's look turned blue, circle B's as is.
import colorsys
import sys

from PIL import Image, ImageDraw, ImageFilter

S = 8          # drawn at 8x, scaled down
N = 64         # texture size
C = 32.0       # disc centre
R = 20.0       # disc radius (texels 12..52)


def hue(rgb, deg):
    if deg is None:
        return rgb
    h, s, v = colorsys.rgb_to_hsv(*(c / 255 for c in rgb))
    r, g, b = colorsys.hsv_to_rgb(deg / 360, s, v)
    return (round(r * 255), round(g * 255), round(b * 255))


def layer():
    return Image.new("RGBA", (N * S, N * S), (0, 0, 0, 0))


def ell(d, cx, cy, r, **kw):
    d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), **kw)


def symbol_mask(kind, dx=0.0, dy=0.0, k=1.0):
    m = Image.new("L", (N * S, N * S), 0)
    g = ImageDraw.Draw(m)
    cx, cy = C + dx, C + dy
    if kind == "cross":
        a, t = 10.0 * k, 5.2 * k  # half extent, stroke width
        for sx in (-1, 1):
            g.line(((cx - a) * S, (cy - a * sx) * S, (cx + a) * S, (cy + a * sx) * S), fill=255, width=int(t * S))
    elif kind == "circle":
        r, t = 11.5 * k, 4.8 * k
        ell(g, cx, cy, r, outline=255, width=int(t * S))
    elif kind == "square":
        a, t = 9.5 * k, 4.4 * k
        g.rectangle(((cx - a) * S, (cy - a) * S, (cx + a) * S, (cy + a) * S), outline=255, width=int(t * S))
    elif kind == "triangle":
        h = 11.5 * k
        pts = [(cx, cy - h), (cx + h * 1.07, cy + h * 0.78), (cx - h * 1.07, cy + h * 0.78)]
        g.polygon([(x * S, y * S) for x, y in pts], fill=255)
        k = 0.42  # hollow it out
        inner = [(cx + (x - cx) * k, cy + 1.2 + (y - cy) * k) for x, y in pts]
        g.polygon([(x * S, y * S) for x, y in inner], fill=0)
    return m


def button(kind, body, light, sym_top, sym_mid, sym_bot, outer_shadow, out, rim=None, k=1.0, r=None):
    global R
    saved_r = R
    if r:
        R = r
    try:
        _button(kind, body, light, sym_top, sym_mid, sym_bot, outer_shadow, out, rim, k)
    finally:
        R = saved_r


def _button(kind, body, light, sym_top, sym_mid, sym_bot, outer_shadow, out, rim, k):
    img = layer()
    if rim:  # the in-game glyphs' thin dark edge
        ell(ImageDraw.Draw(img), C, C, R + 1.6, fill=rim + (255,))
    if outer_shadow:  # B's soft black shadow round the disc
        sh = layer()
        ell(ImageDraw.Draw(sh), C, C + 0.5, R + 2.5, fill=(0, 0, 0, 120))
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 2.2)))
    disc = layer()
    ell(ImageDraw.Draw(disc), C, C, R, fill=body + (255,))
    hl = layer()  # lighter towards the top
    ImageDraw.Draw(hl).ellipse(((C - 12) * S, (C - 18) * S, (C + 12) * S, (C - 4) * S), fill=light + (200,))
    disc.alpha_composite(hl.filter(ImageFilter.GaussianBlur(S * 3)))
    dmask = Image.new("L", disc.size, 0)
    ell(ImageDraw.Draw(dmask), C, C, R, fill=255)
    disc.putalpha(dmask)
    img.alpha_composite(disc)
    # the symbol's shadow, below and a little right
    shadow = tuple(int(c * 0.55) for c in body)
    sm = symbol_mask(kind, 0.6, 1.6, k).filter(ImageFilter.GaussianBlur(S * 0.9))
    sl = Image.new("RGBA", img.size, shadow + (255,))
    sl.putalpha(sm.point(lambda v: v * 0.9))
    img.alpha_composite(sl)
    # the symbol, light at the top, darker at the bottom
    grad = Image.new("RGBA", img.size)
    gd = ImageDraw.Draw(grad)
    y0, y1 = (C - 12) * S, (C + 12) * S
    for yy in range(img.size[1]):
        t = min(1.0, max(0.0, (yy - y0) / (y1 - y0)))
        c = sym_top if t < 0.2 else sym_mid if t < 0.78 else sym_bot
        gd.line((0, yy, img.size[0], yy), fill=c + (255,))
    grad.putalpha(symbol_mask(kind, k=k))
    img.alpha_composite(grad)
    img = img.resize((N, N), Image.LANCZOS)
    img.save(out)
    print(out)


GREEN = dict(body=(74, 97, 57), light=(95, 119, 76), sym_top=(172, 198, 148), sym_mid=(133, 168, 112),
             sym_bot=(98, 128, 82))
RED = dict(body=(84, 41, 41), light=(98, 48, 49), sym_top=(206, 140, 139), sym_mid=(189, 118, 117),
           sym_bot=(140, 73, 74))


def tint(p, deg):
    return {k: hue(v, deg) for k, v in p.items()}


# In-game button glyphs (the font atlas, 28 and 14 px cells): more saturated, with a dark edge. Colours measured from
# the game's glyphs: X blue disc (57,66,98), letter (123,138,180); B red (115,40,41) / (205,101,98); A green
# (74,97,57) / (131,170,115). Cross takes X's blue, circle B's red, triangle A's green, square A's turned pink.
G_BLUE = dict(body=(57, 66, 98), light=(71, 84, 125), sym_top=(160, 175, 215), sym_mid=(123, 138, 180),
              sym_bot=(87, 99, 134))
G_RED = dict(body=(115, 40, 41), light=(139, 48, 49), sym_top=(232, 140, 141), sym_mid=(205, 101, 98),
             sym_bot=(150, 62, 60))
G_GREEN = dict(body=(74, 97, 57), light=(82, 105, 65), sym_top=(180, 202, 156), sym_mid=(131, 170, 115),
               sym_bot=(101, 132, 81))


def grey_disc(kind, out, dark=False):
    # START/BACK in the HUD font: a light grey disc (191,192,191), lighter at the top, dark grey edge (90,89,90),
    # symbol in dark grey (82,76,74). OPTIONS: three lines; touch pad (BACK): a rounded bar.
    img = layer()
    g = ImageDraw.Draw(img)
    ell(g, C, C, R + 1.6, fill=(30, 31, 30, 255) if dark else (70, 69, 70, 255))
    disc = layer()
    ell(ImageDraw.Draw(disc), C, C, R, fill=(84, 84, 84, 255) if dark else (178, 180, 178, 255))
    hl = layer()
    ImageDraw.Draw(hl).ellipse(((C - 13) * S, (C - 17) * S, (C + 13) * S, (C + 2) * S),
                               fill=(130, 128, 130, 200) if dark else (246, 242, 238, 220))
    disc.alpha_composite(hl.filter(ImageFilter.GaussianBlur(S * 3)))
    m = Image.new("L", disc.size, 0)
    ell(ImageDraw.Draw(m), C, C, R, fill=255)
    disc.putalpha(m)
    img.alpha_composite(disc)
    sym = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(sym)
    if kind == "options":
        for dy in (-7, 0, 7):
            d.rounded_rectangle(((C - 10) * S, (C + dy - 2) * S, (C + 10) * S, (C + dy + 2) * S), radius=2 * S, fill=255)
    else:
        d.rounded_rectangle(((C - 12) * S, (C - 7) * S, (C + 12) * S, (C + 7) * S), radius=4 * S, fill=255)
    fg = Image.new("RGBA", img.size, (190, 190, 190, 255) if dark else (82, 76, 74, 255))
    fg.putalpha(sym)
    img.alpha_composite(fg)
    img.resize((N, N), Image.LANCZOS).save(out)
    print(out)


def dark_pill(text, out):
    # The tutorial box's LB/LT pictures (64x64): a dark grey rounded block with a black edge and light grey letters,
    # (90,90,90) / (197,198,197), over a soft shadow.
    from PIL import ImageFont
    n = 64 * S
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    sh = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle((9 * S, 12 * S, 57 * S, 55 * S), radius=11 * S, fill=(0, 0, 0, 150))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 2)))
    g = ImageDraw.Draw(img)
    g.rounded_rectangle((8 * S, 10 * S, 56 * S, 53 * S), radius=11 * S, fill=(30, 32, 30, 255))
    g.rounded_rectangle((10 * S, 12 * S, 54 * S, 51 * S), radius=9 * S, fill=(88, 89, 88, 255))
    top = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(top).rounded_rectangle((12 * S, 13 * S, 52 * S, 28 * S), radius=8 * S, fill=(130, 130, 130, 90))
    img.alpha_composite(top.filter(ImageFilter.GaussianBlur(S * 2)))
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf", int(30 * S))
    b = g.textbbox((0, 0), text, font=f)
    x, y = (n - (b[2] - b[0])) / 2 - b[0], 31.5 * S - (b[3] - b[1]) / 2 - b[1]
    g.text((x + S, y + S), text, font=f, fill=(25, 25, 25, 255))
    g.text((x, y), text, font=f, fill=(197, 198, 197, 255))
    img.resize((64, 64), Image.LANCZOS).save(out)
    print(out)


def pill(text, out, w=64, h=46):
    # LB/RB-style: a light grey rounded box with the label in dark grey (the game's (218,219,218) / (131,129,131)).
    from PIL import ImageFont
    img = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))
    g = ImageDraw.Draw(img)
    g.rounded_rectangle((0, 0, w * S - 1, h * S - 1), radius=10 * S, fill=(150, 150, 150, 255))
    g.rounded_rectangle((2 * S, 2 * S, w * S - 2 * S, h * S - 2 * S), radius=8 * S, fill=(216, 218, 216, 255))
    f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf", int(36 * S))
    b = g.textbbox((0, 0), text, font=f)
    g.text(((w * S - (b[2] - b[0])) / 2 - b[0], (h * S - (b[3] - b[1])) / 2 - b[1]), text, font=f,
           fill=(131, 129, 131, 255))
    img.resize((w, h), Image.LANCZOS).save(out)
    print(out)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "prompts"
    button("cross", **tint(GREEN, 212), outer_shadow=False, out=out + "/ps_cross.png")
    button("circle", **RED, outer_shadow=True, out=out + "/ps_circle.png")
    button("square", **tint(GREEN, 320), outer_shadow=False, out=out + "/ps_square.png")
    button("triangle", **GREEN, outer_shadow=False, out=out + "/ps_triangle.png")
    rim = (12, 10, 10)
    button("cross", **G_BLUE, outer_shadow=False, rim=rim, k=1.3, out=out + "/g_cross.png")
    button("circle", **G_RED, outer_shadow=False, rim=rim, k=1.3, out=out + "/g_circle.png")
    button("square", **tint(G_GREEN, 320), outer_shadow=False, rim=rim, k=1.3, out=out + "/g_square.png")
    button("triangle", **G_GREEN, outer_shadow=False, rim=rim, k=1.3, out=out + "/g_triangle.png")
    for t in ("L1", "R1", "L2", "R2"):  # LS/RS keep the game's stick glyphs
        pill(t, out + "/g_" + t.lower() + ".png")
    grey_disc("options", out + "/g_options.png")
    grey_disc("touchpad", out + "/g_touchpad.png")
    # Whole-texture replacements for the tutorial box and takedown pictures (64x64): the X disc (40 px, and the
    # takedown's 50 px one), LB/LT.
    button("square", **tint(G_GREEN, 320), outer_shadow=False, rim=rim, k=1.3, out=out + "/t_square.png")
    button("square", **tint(G_GREEN, 320), outer_shadow=False, rim=rim, k=1.6, r=24.5, out=out + "/t_square_big.png")
    # The takedown's / tutorial's other discs (50 px, as the large X): A cross, B circle, Y triangle.
    button("cross", **G_BLUE, outer_shadow=False, rim=rim, k=1.6, r=24.5, out=out + "/t_cross_big.png")
    button("circle", **G_RED, outer_shadow=False, rim=rim, k=1.6, r=24.5, out=out + "/t_circle_big.png")
    button("triangle", **G_GREEN, outer_shadow=False, rim=rim, k=1.6, r=24.5, out=out + "/t_triangle_big.png")
    for t in ("L1", "L2", "R2", "R1"):
        dark_pill(t, out + "/t_" + t.lower() + ".png")
    grey_disc("touchpad", out + "/t_touchpad.png", dark=True)  # the tutorial's dark BACK disc
