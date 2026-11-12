#!/usr/bin/env python3
"""Builds the brand pictures from the app's own renders.

  Riffhouse --snapshots <folder> 2     writes icon.png, icon-small.png and cutouts/ (each piece of gear)
  python3 tools/brand/make_assets.py <folder>

Writes the installer wizard pictures into packaging/, and the README banner and the social
preview into docs/, from the logo in resources/brand (built by tools/brand/logo, which also
makes packaging/riffhouse.ico). Needs Pillow and numpy.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SHOTS = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "screenshots"
FONTS = ROOT / "resources" / "fonts"
INK, DIM, BG, AMBER = (236, 233, 227), (165, 161, 153), (14, 14, 14), (255, 163, 26)
LINE = "Free guitar amp and pedalboard"

BRAND = ROOT / "resources" / "brand"


def mark(size, ink=None):
    """The amp-face badge (tools/brand/logo), scaled to size, on a clear background."""
    return Image.open(BRAND / "mark-512.png").convert("RGBA").resize((size, size), Image.LANCZOS)


def name_badge(height):
    """The cream script name with its swash, as the 3D badge, scaled to a height."""
    im = Image.open(BRAND / "wordmark-3d.png").convert("RGBA")
    im = im.crop(im.split()[3].getbbox())
    return im.resize((round(im.width * height / im.height), height), Image.LANCZOS)


def tracked(draw, xy, text, font, fill, tracking):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill)
        x += draw.textlength(ch, font=font) + tracking
    return x


def lockup(canvas, x, y, cap):
    """Badge and name side by side; cap sets the size (the old capital height), y the baseline."""
    m = int(cap * 1.75)
    canvas.alpha_composite(mark(m), (x, int(y - m * 0.92)))
    name = name_badge(int(m * 1.05))
    canvas.alpha_composite(name, (x + m + int(cap * 0.35), int(y - m * 0.92 - m * 0.02)))
    return x + m + int(cap * 0.35) + name.width


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
    # packaging/riffhouse.ico comes straight from tools/brand/logo (photographed 48 px and up, drawn below)
    # the tall picture on the installer's first and last pages, and the small one on the others
    for name, (w, h), m in (("wizard.bmp", (410, 797), 230), ("wizard-small.bmp", (138, 140), 112)):
        im = Image.new("RGBA", (w, h), (21, 20, 19, 255))
        g = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ImageDraw.Draw(g).rectangle([0, 0, w, h // 2], fill=(255, 255, 255, 10))
        im.alpha_composite(g.filter(ImageFilter.GaussianBlur(h // 6)))
        im.alpha_composite(mark(m), ((w - m) // 2, int(h * (0.18 if h > w else 0.10))))
        if h > w * 1.5:
            script = name_badge(118)
            if script.width > w - 40:
                script = script.resize((w - 40, round(script.height * (w - 40) / script.width)), Image.LANCZOS)
            top = int(h * 0.18) + m + 20
            im.alpha_composite(script, ((w - script.width) // 2, top))
            d = ImageDraw.Draw(im)
            sub = ImageFont.truetype(str(FONTS / "InstrumentSans-Medium.ttf"), 25)
            line = "Free guitar amp and pedalboard"
            d.text(((w - d.textlength(line, font=sub)) / 2, top + script.height + 18), line, font=sub, fill=DIM)
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
