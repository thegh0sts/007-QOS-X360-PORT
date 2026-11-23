# tools/make_quit_hint.py <out dir>: draws the title screen's prompts, "A SELECT" over "B QUIT (HOLD)" (start_prompts.png,
# 2026-11-23: the main menu's two lines, with B's label changed), and the older one-line "B QUIT (HOLD)" hint (our own art, no game assets) in the style
# of the game's own button prompts ("A SELECT" / "B BACK" under the main menu): a dark maroon disc with a pink,
# bevelled B, and the label in the prompts' grey, upper case and letter-spaced. Sizes are 1080p pixels, measured from
# a 4K screenshot of the main menu (2026-11-20): disc 27 px, label cap height 13.5 px starting 39.5 px right of the
# disc's left edge, its top 6.5 px below the disc's top. hints/b_quit_ps.png is the same with a PlayStation circle.
# The second line is 45 px below the first (the main menu's B BACK under A SELECT, measured from a 4K screenshot on
# 2026-11-23: disc centres 90 px apart, discs 60 px); the A disc's colours are from the same screenshot. Placed at
# 353,921 (1080p), so the discs line up with the main menu's (707-766 x, 1844-1903 y at 4K).
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

S = 8                         # drawn at 8x, scaled down
D = 30.0                      # disc diameter (main menu 2026-11-23: 60 px at 4K; was 29 for the first hint)
PAD = 1.0                     # margin round the disc (the image starts PAD left of and above the disc)
TEXT_X, CAP_TOP, CAP_H = 40.5, 7.5, 13.5   # from the image's disc box (PAD in)
TRACK = 0.116                 # letter spacing, in em (fits "SELECT" to the game's 73.5 px)
GREY = (78, 86, 89, 255)      # the prompts' label colour
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"  # narrow, as the game's font
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


# Disc colours: dark rim, body, top light; the letter's bevel (top, middle, bottom) and its shadow.
PALETTE = {
    "B": dict(rim=(34, 11, 11), body=(74, 27, 27), light=(120, 60, 60), bevel=((196, 128, 127), (181, 103, 103),
              (140, 73, 74)), shadow=(25, 6, 6)),
    "A": dict(rim=(16, 30, 12), body=(58, 88, 42), light=(110, 150, 85), bevel=((186, 222, 158), (150, 194, 118),
              (100, 142, 74)), shadow=(10, 22, 8)),
    # PlayStation: the cross in blue (as prompts/ps_cross.png: A's look turned blue), the circle on B's maroon
    "X": dict(rim=(10, 18, 36), body=(36, 58, 98), light=(90, 120, 175), bevel=((170, 196, 240), (128, 160, 220),
              (86, 112, 170)), shadow=(6, 10, 24)),
    "O": None,
}


def disc(symbol):
    n = int((D + 2 * PAD) * S)
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    g = ImageDraw.Draw(img)
    o, d = PAD * S, D * S
    pal = PALETTE.get(symbol) or PALETTE["B"]
    g.ellipse((o, o, o + d, o + d), fill=pal["rim"] + (255,))                     # dark rim
    g.ellipse((o + 1.2 * S, o + 1.2 * S, o + d - 1.2 * S, o + d - 1.2 * S), fill=pal["body"] + (255,))
    # a little lighter towards the top, as the game's discs
    top = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(top).ellipse((o + d * 0.2, o + d * 0.08, o + d * 0.8, o + d * 0.5), fill=pal["light"] + (60,))
    img.alpha_composite(top.filter(ImageFilter.GaussianBlur(S * 1.5)))
    cx, cy = o + d / 2, o + d / 2
    if symbol == "X":  # PlayStation cross: two bars in the light blue, with a shadow
        arm, w = d * 0.24, 2.8 * S
        for dx, dy, col in ((0.6 * S, 0.9 * S, pal["shadow"] + (230,)), (0, 0, pal["bevel"][0] + (255,))):
            g.line((cx - arm + dx, cy - arm + dy, cx + arm + dx, cy + arm + dy), fill=col, width=int(w))
            g.line((cx - arm + dx, cy + arm + dy, cx + arm + dx, cy - arm + dy), fill=col, width=int(w))
    elif symbol in ("A", "B"):
        f = ImageFont.truetype(BOLD, int(d * 0.80))
        box = g.textbbox((0, 0), symbol, font=f)
        w, h = box[2] - box[0], box[3] - box[1]
        x, y = cx - w / 2 - box[0], cy - h / 2 - box[1]
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).text((x + 0.8 * S, y + 1.0 * S), symbol, font=f, fill=pal["shadow"] + (230,))  # drop shadow
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 0.4)))
        # the letter: lighter at the top, darker at the bottom (the game's bevel)
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).text((x, y), symbol, font=f, fill=255)
        grad = Image.new("RGBA", img.size)
        gd = ImageDraw.Draw(grad)
        y0, y1 = int(y + box[1]), int(y + box[3])
        for yy in range(img.size[1]):
            t = min(1.0, max(0.0, (yy - y0) / max(1, y1 - y0)))
            c = pal["bevel"][0] if t < 0.18 else pal["bevel"][1] if t < 0.8 else pal["bevel"][2]
            gd.line((0, yy, img.size[0], yy), fill=c + (255,))
        img.paste(grad, (0, 0), mask)
    else:  # PlayStation circle: a ring in the same pink
        r = d * 0.27
        g.ellipse((cx - r + 0.5 * S, cy - r + 0.8 * S, cx + r + 0.5 * S, cy + r + 0.8 * S), outline=(25, 6, 6, 230),
                  width=int(2.6 * S))
        g.ellipse((cx - r, cy - r, cx + r, cy + r), outline=(190, 112, 112, 255), width=int(2.6 * S))
    return img


def label(text):
    # font size so that the capitals are CAP_H tall
    probe = ImageFont.truetype(REG, 100 * S)
    b = ImageDraw.Draw(Image.new("L", (1, 1))).textbbox((0, 0), "H", font=probe)
    size = 100 * S * CAP_H * S / (b[3] - b[1])
    f = ImageFont.truetype(REG, int(round(size)))
    return f, size * TRACK


LINE = 45.0  # 1080p px from one prompt line to the next (the main menu's A SELECT / B BACK: 90 px at 4K)


def text_width(text):
    f, track = label(text)
    g0 = ImageDraw.Draw(Image.new("L", (1, 1)))
    return sum(g0.textlength(ch, font=f) for ch in text) + track * (len(text) - 1)


def draw_line(img, top, symbol, text):
    """One prompt (disc + label) with its image box's top `top` 1080p px down."""
    f, track = label(text)
    g0 = ImageDraw.Draw(Image.new("L", (1, 1)))
    img.alpha_composite(disc(symbol), (0, int(round(top * S))))
    hb = g0.textbbox((0, 0), "H", font=f)
    x = (PAD + TEXT_X) * S
    y = (top + PAD + CAP_TOP) * S - hb[1]
    g = ImageDraw.Draw(img)
    for ch in text:
        g.text((x, y), ch, font=f, fill=GREY)
        x += g0.textlength(ch, font=f) + track


def prompts(lines, out):
    """lines: (symbol, label) from the top, LINE apart."""
    W = int(round(PAD + TEXT_X + max(text_width(t) for _, t in lines) / S + 3))
    H = int(round(LINE * (len(lines) - 1) + D + 2 * PAD))
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    for i, (symbol, text) in enumerate(lines):
        draw_line(img, LINE * i, symbol, text)
    img = img.resize((W, H), Image.LANCZOS)
    img.save(out)
    print(out, img.size, W * H, "pixels")


def hint(symbol, text, out):
    prompts([(symbol, text)], out)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "hints"
    # The title screen's (src/menu_quit.cpp): A SELECT over B QUIT (HOLD), and the PlayStation pair.
    prompts([("A", "SELECT"), ("B", "QUIT (HOLD)")], out + "/start_prompts.png")
    prompts([("X", "SELECT"), ("O", "QUIT (HOLD)")], out + "/start_prompts_ps.png")
