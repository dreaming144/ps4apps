#!/usr/bin/env python3
"""Original shelf covers — recognizable, not scans of the published jackets."""
from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(__file__), "assets")
os.makedirs(OUT, exist_ok=True)


def font(size, bold=False):
    for p in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ):
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def save(im, name):
    path = os.path.join(OUT, name)
    im.save(path, "PNG")
    print("wrote", path, im.size)


def spirit():
    W, H = 300, 460
    im = Image.new("RGB", (W, H), (92, 38, 28))
    px = im.load()
    for y in range(H):
        for x in range(W):
            t = y / (H - 1)
            r = int(118 + 40 * t)
            g = int(48 + 18 * t)
            b = int(32 + 8 * (1 - t))
            if x < 22:
                r, g, b = r - 22, g - 10, b - 8
            px[x, y] = (max(0, r), max(0, g), max(0, b))
    d = ImageDraw.Draw(im, "RGBA")
    cx, cy = 168, 168
    for i, a in ((92, 40), (70, 80), (48, 140)):
        d.ellipse([cx - i, cy - i, cx + i, cy + i], outline=(232, 184, 109, a), width=4)
    d.ellipse([cx - 18, cy - 18, cx + 18, cy + 18], fill=(232, 184, 109, 230))
    d.polygon([(40, 300), (80, 250), (120, 292), (160, 240), (210, 288), (250, 248), (290, 310), (290, 360), (40, 360)], fill=(36, 56, 40, 210))
    d.rectangle([36, 372, 274, 376], fill=(232, 184, 109, 220))
    fb, fm = font(22, True), font(13)
    d.text((36, 384), "SPIRIT", font=fb, fill=(244, 232, 200))
    d.text((36, 410), "MEDICINE", font=fb, fill=(232, 184, 109))
    d.text((36, 438), "Wesselman  ·  Kuykendall", font=fm, fill=(210, 180, 150))
    save(im, "cover-spirit.png")


def subtle():
    W, H = 300, 460
    im = Image.new("RGB", (W, H), (232, 92, 36))
    px = im.load()
    for y in range(H):
        for x in range(W):
            t = y / (H - 1)
            r = int(236 - 18 * t)
            g = int(98 - 16 * t)
            b = int(38 - 6 * t)
            if x < 22:
                r, g, b = r - 28, g - 14, b - 8
            px[x, y] = (r, g, b)
    d = ImageDraw.Draw(im, "RGBA")
    d.rectangle([22, 0, 26, H], fill=(20, 16, 14, 80))
    fb, f2, fm = font(28, True), font(26, True), font(13, True)
    y = 86
    for line in ("THE", "SUBTLE", "ART OF", "NOT", "GIVING", "A F*CK"):
        d.text((40, y), line, font=fb if len(line) < 8 else f2, fill=(255, 255, 255))
        y += 42
    d.rectangle([40, 360, 160, 364], fill=(20, 16, 14, 220))
    d.text((40, 380), "MARK MANSON", font=fm, fill=(20, 16, 14))
    d.text((40, 404), "A COUNTERINTUITIVE", font=font(11), fill=(40, 22, 16))
    d.text((40, 422), "APPROACH TO A GOOD LIFE", font=font(11), fill=(40, 22, 16))
    save(im, "cover-subtle.png")


def power():
    W, H = 300, 460
    im = Image.new("RGB", (W, H), (10, 10, 12))
    px = im.load()
    for y in range(H):
        for x in range(W):
            v = 10 + int(8 * (y / (H - 1)))
            if x < 22:
                px[x, y] = (90, 14, 18)
            else:
                px[x, y] = (v, v, v + 2)
    d = ImageDraw.Draw(im, "RGBA")
    d.rectangle([22, 0, 26, H], fill=(160, 28, 32, 200))
    fh, fm, fs = font(120, True), font(16, True), font(12)
    d.text((48, 70), "48", font=fh, fill=(196, 32, 38))
    d.rectangle([52, 220, 248, 223], fill=(212, 168, 92, 220))
    d.text((52, 240), "THE 48 LAWS", font=fm, fill=(236, 228, 214))
    d.text((52, 264), "OF POWER", font=fm, fill=(212, 168, 92))
    d.text((52, 400), "ROBERT GREENE", font=fs, fill=(180, 176, 168))
    d.text((52, 420), "A JOOST ELFFERS BOOK", font=font(10), fill=(90, 88, 84))
    save(im, "cover-power.png")


if __name__ == "__main__":
    spirit()
    subtle()
    power()
