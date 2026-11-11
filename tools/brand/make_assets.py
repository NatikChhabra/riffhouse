#!/usr/bin/env python3
"""Builds the brand pictures from the app's own renders.

  Riffhouse --snapshots <folder> 2     writes icon.png, icon-small.png and cutouts/ (each piece of gear)
  python3 tools/brand/make_assets.py <folder>

Writes the installer icon and wizard pictures into packaging/, and the README banner and the
social preview into docs/. Needs Pillow and numpy.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SHOTS = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "screenshots"
FONTS = ROOT / "resources" / "fonts"
INK, DIM, BG, AMBER = (236, 233, 227), (165, 161, 153), (14, 14, 14), (255, 163, 26)
LINE = "Free guitar amp and pedalboard"

# The mark on a 100 unit square, the same numbers as src/ui/Theme.cpp and resources/brand/mark.svg:
# a guitar pick with a roof over the letter H cut out of it, and the pilot light under the roof.
PICK = [((50, 5), (74, 5), (95, 14), (95, 36)), ((95, 36), (95, 58), (60, 96), (50, 96)),
        ((50, 96), (40, 96), (5, 58), (5, 36)), ((5, 36), (5, 14), (26, 5), (50, 5))]
ROOF = [(50, 21), (79, 45), (67.5, 45), (50, 30.5), (32.5, 45), (21, 45)]
H = [(34.5, 51), (41.5, 51), (41.5, 61), (58.5, 61), (58.5, 51), (65.5, 51), (65.5, 78), (58.5, 78), (58.5, 68), (41.5, 68), (41.5, 78), (34.5, 78)]
LAMP = (240, 138, 28)


def curve(p0, p1, p2, p3, steps=48):
    for i in range(steps):
        t = i / steps
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        yield (a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1])


def mark(size, ink=INK):
    """The mark alone on a clear background, drawn four times too big and scaled down."""
    k = 4
    u = size * k / 100
    mask = Image.new("L", (size * k, size * k), 0)
    d = ImageDraw.Draw(mask)
    d.polygon([(x * u, y * u) for seg in PICK for x, y in curve(*seg)], fill=255)
    for hole in (ROOF, H):
        d.polygon([(x * u, y * u) for x, y in hole], fill=0)
    im = Image.new("RGBA", mask.size, ink + (0,))
    im.putalpha(mask)
    ImageDraw.Draw(im).ellipse([(50 - 4.4) * u, (42.2 - 4.4) * u, (50 + 4.4) * u, (42.2 + 4.4) * u], fill=LAMP + (255,))
    return im.resize((size, size), Image.LANCZOS)


def tracked(draw, xy, text, font, fill, tracking):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += draw.textlength(ch, font=font) + tracking
    return x


def lockup(canvas, x, y, cap):
    """Mark and name side by side; cap is the height of the capitals in pixels."""
    d = ImageDraw.Draw(canvas)
    font = ImageFont.truetype(str(FONTS / "BigShoulders-ExtraBold.ttf"), int(cap / 0.70))
    m = int(cap * 1.75)
    box = d.textbbox((0, 0), "H", font=font)
    canvas.alpha_composite(mark(m), (x, int(y - m * 0.92 + m * 0.0)))
    return tracked(d, (x + m + int(cap * 0.42), y - box[3]), "RIFFHOUSE", font, INK, cap * 0.23)


def stage(size, light):
    """The dark room with one soft warm light, the same idea as the app's own background."""
    import numpy as np
    w, h = size
    yy, xx = np.mgrid[0:h, 0:w].astype("float32")
    rad = w * 0.55
    t = np.exp(-2.2 * (((xx - light[0]) / rad) ** 2 + ((yy - light[1]) / (rad * 0.8)) ** 2))
    v = 13.0 + 24.0 * t + (np.random.default_rng(17).random((h, w)) - 0.5)
    rgb = np.stack([v + 1.4 * t, v, v - 1.0 * t], axis=-1).clip(0, 255).astype("uint8")
    return Image.fromarray(rgb, "RGB").convert("RGBA")


def gear(name, height=None, width=None):
    """One piece of gear cut out of the app, trimmed to its own edge and scaled."""
    im = Image.open(SHOTS / "cutouts" / (name + ".png")).convert("RGBA")
    im = im.crop(im.split()[3].point(lambda a: 255 if a > 40 else 0).getbbox())
    k = height / im.height if height else width / im.width
    return im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)


def stand(canvas, piece, x, floor):
    """Puts a piece on the floor with the shadow it throws there and a faint reflection in the floor's polish."""
    w, h = piece.size
    sh = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([x - w * 0.04, floor - h * 0.035, x + w * 1.10, floor + h * 0.06], fill=(0, 0, 0, 215))
    canvas.alpha_composite(sh.filter(ImageFilter.GaussianBlur(max(4, h // 22))))
    depth = min(h, max(24, h // 4))
    mirror = piece.transpose(Image.FLIP_TOP_BOTTOM).crop((0, 0, w, depth))
    fade = Image.linear_gradient("L").resize((w, depth)).point(lambda v: int((255 - v) * 0.16))
    mirror.putalpha(Image.composite(fade, Image.new("L", (w, depth), 0), mirror.split()[3]))
    canvas.alpha_composite(mirror.filter(ImageFilter.GaussianBlur(1.2)), (round(x), round(floor)))
    canvas.alpha_composite(piece, (round(x), round(floor - h)))


def floorboards(canvas, horizon):
    """A floor to stand on: it starts at a soft edge and catches a little of the room's light."""
    w, h = canvas.size
    band = Image.new("RGBA", (w, h - horizon), (0, 0, 0, 0))
    d = ImageDraw.Draw(band)
    for y in range(band.height):
        t = y / band.height
        d.line([(0, y), (w, y)], fill=(255, 244, 226, int(16 * (1 - t) ** 2)))
    d.line([(0, 0), (w, 0)], fill=(255, 244, 226, 30))
    canvas.alpha_composite(band, (0, horizon))


def row(canvas, names, left, right, floor, height):
    """A line of pedals spread evenly between two edges."""
    while True:  # shrink until there is air between them
        pieces = [gear(n, height=height) for n in names]
        gap = (right - left - sum(p.width for p in pieces)) / max(1, len(pieces) - 1)
        if gap >= height * 0.09:
            break
        height -= 4
    x = left
    for p in pieces:
        stand(canvas, p, x, floor)
        x += p.width + gap


def cover(size, name, light, cap, text_xy, line_px, amp, pedals, horizon=None):
    im = stage(size, light)
    if horizon:
        floorboards(im, horizon)
    if amp:
        piece, x, floor = amp
        stand(im, piece, x, floor)
    names, left, right, floor, height = pedals
    row(im, names, left, right, floor, height)
    x, y = text_xy
    lockup(im, x, y, cap)
    d = ImageDraw.Draw(im)
    font = ImageFont.truetype(str(FONTS / "InstrumentSans-Medium.ttf"), line_px)
    d.text((x + 2, y + int(cap * 0.62)), LINE, font=font, fill=INK)
    out = ROOT / "docs" / name
    im.convert("RGB").save(out, optimize=True)
    print("wrote", out.relative_to(ROOT))


def installer_art():
    big = Image.open(SHOTS / "icon.png").convert("RGBA")
    small = Image.open(SHOTS / "icon-small.png").convert("RGBA")
    frames = [(big if s >= 64 else small).resize((s, s), Image.LANCZOS) for s in (256, 128, 64, 48, 32, 24, 16)]
    out = ROOT / "packaging" / "riffhouse.ico"
    frames[0].save(out, format="ICO", append_images=frames[1:], sizes=[f.size for f in frames])
    print("wrote", out.relative_to(ROOT))
    # the tall picture on the installer's first and last pages, and the small one on the others
    for name, (w, h), m in (("wizard.bmp", (410, 797), 230), ("wizard-small.bmp", (138, 140), 104)):
        im = Image.new("RGBA", (w, h), (21, 20, 19, 255))
        g = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(g).rectangle([0, 0, w, h // 2], fill=(255, 255, 255, 10))
        im.alpha_composite(g.filter(ImageFilter.GaussianBlur(h // 6)))
        im.alpha_composite(mark(m), ((w - m) // 2, int(h * (0.20 if h > w else 0.12))))
        if h > w * 1.5:
            d = ImageDraw.Draw(im)
            font = ImageFont.truetype(str(FONTS / "BigShoulders-ExtraBold.ttf"), 58)
            width = sum(d.textlength(c, font=font) + 9 for c in "RIFFHOUSE") - 9
            tracked(d, ((w - width) / 2, h * 0.20 + m + 26), "RIFFHOUSE", font, INK, 9)
            sub = ImageFont.truetype(str(FONTS / "InstrumentSans-Medium.ttf"), 25)
            line = "Free guitar amp and pedalboard"
            d.text(((w - d.textlength(line, font=sub)) / 2, h * 0.20 + m + 108), line, font=sub, fill=DIM)
        out = ROOT / "packaging" / name
        im.convert("RGB").save(out, format="BMP")
        print("wrote", out.relative_to(ROOT))


if __name__ == "__main__":
    installer_art()
    # pieces that differ in outline, so the row does not read as one box seven times
    board = ["wah-0", "drive-5", "eq-0", "delay-2", "delay-1", "mod-5", "drive-2"]
    # the share picture: name and one line, the amp at the back of the room, the board in front
    cover((1280, 640), "social-preview.png", (900, 240), 56, (64, 250), 36,
          (gear("amp-03", width=560), 672, 404), (board, 64, 1216, 604, 176), horizon=392)
    # the strip on top of the README: shorter, so the board carries it alone
    cover((1280, 440), "banner.png", (860, 200), 54, (64, 198), 30,
          None, ([board[1], board[0], board[3], board[4]], 600, 1216, 372, 250), horizon=300)
