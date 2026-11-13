# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Turn the renderer's pictures into what the app carries.

    python pack.py <render folder> <resources/gear>

A big picture becomes a JPEG for its colour and a grey PNG for how solid each pixel is: a fifth
of the size of one PNG. Knobs and panel hardware stay PNGs. The chicken-head frames become one sheet, 13 across.
"""
import glob
import os
import sys

import numpy as np
from PIL import Image


def fade_edges(im, margin):
    """Let the shadow on the floor die away before the edge of the picture, so no edge shows."""
    a = np.asarray(im.getchannel("A")).astype(np.float32)
    h, w = a.shape
    x = np.minimum(np.arange(w), np.arange(w)[::-1]).astype(np.float32)
    y = np.minimum(np.arange(h), np.arange(h)[::-1]).astype(np.float32)
    f = np.clip(np.minimum(x[None, :], y[:, None]) / margin, 0.0, 1.0)
    f = f * f * (3.0 - 2.0 * f)
    im.putalpha(Image.fromarray((a * f + 0.5).astype(np.uint8), "L"))
    return im


def fade_round(im, inner, outer):
    """The same for a knob: its shadow is gone by the rim of the picture."""
    a = np.asarray(im.getchannel("A")).astype(np.float32)
    h, w = a.shape
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot(xx - (w - 1) / 2.0, yy - (h - 1) / 2.0) / (w / 2.0)
    f = np.clip((outer - r) / (outer - inner), 0.0, 1.0)
    f = f * f * (3.0 - 2.0 * f)
    im.putalpha(Image.fromarray((a * f + 0.5).astype(np.uint8), "L"))
    return im

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
total = 0
for f in sorted(glob.glob(os.path.join(src, "*.png"))):
    name = os.path.splitext(os.path.basename(f))[0].replace("-", "_")
    im = Image.open(f).convert("RGBA")
    if name.startswith(("knob", "fader", "hw_")):
        im = fade_round(im, 0.72 if not name.startswith("hw_") else 0.8, 0.98)
        side = 128 if name.startswith("hw_screw") else 192 if name.startswith("hw_jack") else 256  # panel hardware: small on screen
        im = im.resize((side, side), Image.LANCZOS) if im.width > side else im
        out = os.path.join(dst, name + ".png")
        im.save(out, optimize=True)
        total += os.path.getsize(out)
        continue
    im = fade_edges(im, max(8.0, im.width * 0.045))
    # colour under clear pixels would only waste bytes: paint it black
    rgb = Image.new("RGB", im.size, (0, 0, 0))
    rgb.paste(im, mask=im.getchannel("A"))
    # straight colour where the picture is partly clear, so edges do not go dark twice
    solid = Image.composite(im.convert("RGB"), rgb, im.getchannel("A").point(lambda a: 255 if a > 0 else 0))
    out = os.path.join(dst, name + ".jpg")
    solid.save(out, quality=90, subsampling=0, optimize=True)
    mask = os.path.join(dst, name + "_a.png")
    im.getchannel("A").save(mask, optimize=True)
    total += os.path.getsize(out) + os.path.getsize(mask)
frames = sorted(glob.glob(os.path.join(src, "_chicken", "*.png")))
if frames:
    cols, size = 13, 224
    rows = (len(frames) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * size, rows * size), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        frame = fade_round(Image.open(f).convert("RGBA"), 0.72, 0.98)
        sheet.paste(frame.resize((size, size), Image.LANCZOS), ((i % cols) * size, (i // cols) * size))
    out = os.path.join(dst, "knob_chicken.png")
    sheet.save(out, optimize=True)
    total += os.path.getsize(out)
# strips of opaque frames (a treadle in each of its positions): one row, colour only
for folder in sorted(glob.glob(os.path.join(src, "_strips", "*"))):
    frames = sorted(glob.glob(os.path.join(folder, "*.png")))
    if not frames:
        continue
    ims = [Image.open(f).convert("RGB") for f in frames]
    sheet = Image.new("RGB", (ims[0].width * len(ims), ims[0].height))
    for i, im in enumerate(ims):
        sheet.paste(im, (i * ims[0].width, 0))
    out = os.path.join(dst, os.path.basename(folder) + ".jpg")
    sheet.save(out, quality=90, subsampling=0, optimize=True)
    total += os.path.getsize(out)
print("packed into %s: %.1f MB" % (dst, total / 1e6))
