"""Make the pictures the first screen's pedal is built from.

The first screen draws the pedal as a real box: a face, four walls and a back, with knobs and
a footswitch that stand up off the face as cylinders. So it needs:

  hero-pedal.webp  the face alone, cut to the metal, with the knobs and the footswitch's top
                   painted out (they are built in 3D on top of it, so a flat copy must not show)
  hero-caps.webp   the tops of the three knobs and of the footswitch, round, side by side

    python3 tools/site/hero_pedal.py            # reads site/img/gear/drive-5.webp

Measured on drive-5.webp (758 x 840). The page's CSS uses the same numbers, in face pixels.
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

SITE = Path(__file__).resolve().parents[2] / "site" / "img"
BODY = (30, 1, 650, 778)                      # the metal box, without the jacks and shadow
KNOBS = [(159, 124), (339, 124), (519, 124)]  # skirt centres; skirt radius 54
KNOB_CAP = (-1, -7, 32)                       # the knurled cap sits up and left of the skirt centre
STOMP = (215, 599, 33)                        # footswitch top, inside its hex nut


def fill(img, mask):
    """Paint out the masked pixels with the plain face around them (tick dots left out)."""
    out = img.copy()
    yy, xx = np.nonzero(mask)
    ring = np.zeros_like(mask)
    ring[max(yy.min() - 6, 0):yy.max() + 7, max(xx.min() - 6, 0):xx.max() + 7] = True
    ring &= ~mask
    near = img[ring][:, :3].astype(int)
    plain = near[near.sum(axis=1) > np.percentile(near.sum(axis=1), 40)]
    soft = np.array(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(3))) / 255
    out[..., :3] = (img[..., :3] * (1 - soft[..., None]) + np.median(plain, axis=0) * soft[..., None]).astype(np.uint8)
    return out


def contact(img, cx, cy, r, dx, dy, depth):
    """The soft dark patch a part standing on the face leaves below and right of itself."""
    yy, xx = np.mgrid[:img.shape[0], :img.shape[1]]
    spot = (np.hypot(xx - cx - dx, yy - cy - dy) < r).astype(np.uint8) * 255
    spot = np.array(Image.fromarray(spot).filter(ImageFilter.GaussianBlur(r * .22))) / 255 * depth
    out = img.copy()
    out[..., :3] = (img[..., :3] * (1 - spot[..., None])).astype(np.uint8)
    return out


def disc(img, cx, cy, r):
    crop = Image.fromarray(img[cy - r:cy + r, cx - r:cx + r])
    yy, xx = np.mgrid[:2 * r, :2 * r] + 0.5
    edge = np.clip(r - np.hypot(xx - r, yy - r), 0, 1)
    crop.putalpha(Image.fromarray((edge * 255).astype(np.uint8)))
    return crop.resize((128, 128), Image.LANCZOS)


def main(source="gear/drive-5.webp"):
    src = np.array(Image.open(SITE / source).convert("RGBA"))
    yy, xx = np.mgrid[:src.shape[0], :src.shape[1]]
    face = src
    for cx, cy in KNOBS:                                              # one at a time, so rows
        face = fill(face, (np.hypot(xx - cx, yy - cy) < 62)           # never bridge two knobs
                    | ((abs(xx - cx) < 44) & (yy > cy) & (yy < 206)))  # skirt and its shadow
        face = contact(face, cx, cy, 56, 9, 12, .38)
    face = fill(face, np.hypot(xx - STOMP[0], yy - STOMP[1]) < STOMP[2] + 1)
    face = contact(face, STOMP[0], STOMP[1], STOMP[2], 5, 7, .3)
    face = face[BODY[1]:BODY[3], BODY[0]:BODY[2]].copy()
    face[..., 3] = np.maximum(face[..., 3], 255 * (face[..., 3] > 40))
    Image.fromarray(face).save(SITE / "hero-pedal.webp", quality=90, method=6)

    caps = Image.new("RGBA", (512, 128))
    for i, (cx, cy) in enumerate(KNOBS):
        caps.paste(disc(src, cx + KNOB_CAP[0], cy + KNOB_CAP[1], KNOB_CAP[2]), (128 * i, 0))
    caps.paste(disc(src, *STOMP), (384, 0))
    caps.save(SITE / "hero-caps.webp", quality=88, method=6)
    for name in ("hero-pedal.webp", "hero-caps.webp"):
        print(name, (SITE / name).stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main()
