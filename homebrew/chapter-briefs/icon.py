#!/usr/bin/env python3
"""Original Chapter Briefs icon — three standing books. No console marks."""
from PIL import Image, ImageDraw

W = 512
im = Image.new("RGB", (W, W), (10, 11, 14))
px = im.load()
for y in range(W):
    for x in range(W):
        t = y / (W - 1)
        s = x / (W - 1)
        r = int(12 + 10 * t + 6 * s)
        g = int(11 + 8 * t)
        b = int(16 + 10 * (1 - t) + 4 * s)
        px[x, y] = (r, g, b)

d = ImageDraw.Draw(im, "RGBA")


def book(x, y, w, h, cover, foil, spine):
    d.rounded_rectangle([x + 8, y + 12, x + w + 8, y + h + 12], radius=8, fill=(0, 0, 0, 70))
    d.rounded_rectangle([x, y, x + w, y + h], radius=8, fill=cover)
    d.rectangle([x, y + 8, x + 14, y + h - 8], fill=spine)
    d.rounded_rectangle([x + 28, y + 28, x + w - 14, y + 38], radius=3, fill=foil)
    d.rounded_rectangle([x + 28, y + 48, x + w - 36, y + 56], radius=2, fill=foil)
    d.rounded_rectangle([x + 28, y + h - 48, x + w - 20, y + h - 22], radius=5, fill=(250, 244, 230, 230))
    for i in range(4):
        yy = y + h - 42 + i * 4
        d.rectangle([x + 36, yy, x + w - 28, yy + 1], fill=(40, 36, 32, 80))


book(46, 96, 132, 280, (122, 58, 42, 255), (232, 184, 109, 220), (88, 38, 28, 255))
book(190, 118, 132, 258, (36, 44, 56, 255), (232, 122, 64, 220), (22, 28, 36, 255))
book(334, 86, 132, 290, (92, 28, 36, 255), (212, 168, 92, 220), (64, 18, 24, 255))
d.rounded_rectangle([40, 430, 472, 464], radius=10, fill=(22, 24, 28, 220))
d.rounded_rectangle([52, 438, 168, 456], radius=5, fill=(232, 184, 109, 210))
d.rounded_rectangle([184, 438, 300, 456], radius=5, fill=(232, 122, 64, 180))
d.rounded_rectangle([316, 438, 432, 456], radius=5, fill=(196, 72, 80, 180))
im.save("sce_sys/icon0.png")
print("wrote sce_sys/icon0.png")
