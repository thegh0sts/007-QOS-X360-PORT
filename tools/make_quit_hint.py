# tools/make_quit_hint.py <out dir>: draws the "B QUIT (HOLD)" title-screen hint (our own art, no game assets) in the style
# of the game's own button prompts ("A SELECT" / "B BACK" under the main menu): a dark maroon disc with a pink,
# bevelled B, and the label in the prompts' grey, upper case and letter-spaced. Sizes are 1080p pixels, measured from
# a 4K screenshot of the main menu (2026-11-20): disc 27 px, label cap height 13.5 px starting 39.5 px right of the
# disc's left edge, its top 6.5 px below the disc's top. hints/b_quit_ps.png is the same with a PlayStation circle.
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

S = 8                         # drawn at 8x, scaled down
D = 29.0                      # disc diameter (27 measured inside its dark rim; 29 matches how big it looks)
PAD = 1.0                     # margin round the disc (the image starts PAD left of and above the disc)
TEXT_X, CAP_TOP, CAP_H = 40.5, 7.5, 13.5   # from the image's disc box (PAD in)
TRACK = 0.116                 # letter spacing, in em (fits "SELECT" to the game's 73.5 px)
GREY = (78, 86, 89, 255)      # the prompts' label colour
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed.ttf"  # narrow, as the game's font
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def disc(symbol):
    n = int((D + 2 * PAD) * S)
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    g = ImageDraw.Draw(img)
    o, d = PAD * S, D * S
    g.ellipse((o, o, o + d, o + d), fill=(34, 11, 11, 255))                       # dark rim
    g.ellipse((o + 1.2 * S, o + 1.2 * S, o + d - 1.2 * S, o + d - 1.2 * S), fill=(74, 27, 27, 255))
    # a little lighter towards the top, as the game's discs
    top = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(top).ellipse((o + d * 0.2, o + d * 0.08, o + d * 0.8, o + d * 0.5), fill=(120, 60, 60, 60))
    img.alpha_composite(top.filter(ImageFilter.GaussianBlur(S * 1.5)))
    cx, cy = o + d / 2, o + d / 2
    if symbol == "B":
        f = ImageFont.truetype(BOLD, int(d * 0.80))
        box = g.textbbox((0, 0), "B", font=f)
        w, h = box[2] - box[0], box[3] - box[1]
        x, y = cx - w / 2 - box[0], cy - h / 2 - box[1]
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ImageDraw.Draw(sh).text((x + 0.8 * S, y + 1.0 * S), "B", font=f, fill=(25, 6, 6, 230))  # drop shadow
        img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(S * 0.4)))
        # the letter: lighter at the top, darker at the bottom (the game's bevel)
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).text((x, y), "B", font=f, fill=255)
        grad = Image.new("RGBA", img.size)
        gd = ImageDraw.Draw(grad)
        y0, y1 = int(y + box[1]), int(y + box[3])
        for yy in range(img.size[1]):
            t = min(1.0, max(0.0, (yy - y0) / max(1, y1 - y0)))
            c = (196, 128, 127) if t < 0.18 else (181, 103, 103) if t < 0.8 else (140, 73, 74)
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


def hint(symbol, text, out):
    f, track = label(text)
    g0 = ImageDraw.Draw(Image.new("L", (1, 1)))
    widths = [g0.textlength(ch, font=f) for ch in text]
    text_w = sum(widths) + track * (len(text) - 1)
    W = int(round(PAD + TEXT_X + text_w / S + 3))
    H = int(round(D + 2 * PAD))
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    img.alpha_composite(disc(symbol), (0, 0))
    hb = g0.textbbox((0, 0), "H", font=f)
    x = (PAD + TEXT_X) * S
    y = (PAD + CAP_TOP) * S - hb[1]
    g = ImageDraw.Draw(img)
    for ch, w in zip(text, widths):
        g.text((x, y), ch, font=f, fill=GREY)
        x += w + track
    img = img.resize((W, H), Image.LANCZOS)
    img.save(out)
    print(out, img.size)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "hints"
    hint("B", "QUIT (HOLD)", out + "/b_quit.png")
    hint("O", "QUIT (HOLD)", out + "/b_quit_ps.png")
