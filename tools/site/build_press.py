#!/usr/bin/env python3
"""Puts the press kit on the website.

  python3 tools/site/build_press.py <kit folder> [<snapshot folder>]

<kit folder> is the press kit as it is handed out: README.md, logo/ and screenshots/.
Copies it to site/press/ and writes the small pictures the page shows to site/img/press/.
A screenshot named in SWAP is taken from the snapshot folder instead, at the kit's size:
those two in the kit still show the old logo, and one lists a tone named after a player.
The zip that the page offers is made when the site is published (see pages.yml).
"""
import shutil
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PRESS = ROOT / "site" / "press"
SMALL = ROOT / "site" / "img" / "press"
SWAP = {"takes": "takes", "take-tone": "take-tone"}
# How each logo is shown on the page: the ground it sits on, and the width of its small picture.
LOGOS = {"mark-1024": 880, "wordmark-flat-cream": 800, "wordmark-flat-ink": 800, "lockup-transparent": 900, "social-card-1200x630": 900}


def small(src, dst, width):
    im = Image.open(src)
    im = im.convert("RGBA" if "A" in im.getbands() else "RGB")
    if im.width > width:
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
    im.save(dst, "WEBP", quality=84, method=6)


def main(kit, shots=None):
    for sub in ("logo", "screenshots"):
        (PRESS / sub).mkdir(parents=True, exist_ok=True)
    SMALL.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(kit / "README.md", PRESS / "README.md")
    for f in sorted((kit / "logo").iterdir()):
        shutil.copyfile(f, PRESS / "logo" / f.name)
        if f.stem in LOGOS:
            small(f, SMALL / f"logo-{f.stem}.webp", LOGOS[f.stem])
    names = []
    for f in sorted((kit / "screenshots").glob("*.png")):
        out = PRESS / "screenshots" / f.name
        new = shots / f"{SWAP[f.stem]}.png" if shots and f.stem in SWAP else None
        if new and new.is_file():
            with Image.open(f) as old:
                size = old.size
            Image.open(new).convert("RGB").resize(size, Image.LANCZOS).save(out, optimize=True)
        else:
            shutil.copyfile(f, out)
        small(out, SMALL / f"{f.stem}.webp", 900)
        names.append(f.stem)
    print(len(names), "screenshots:", ", ".join(names))


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]) if len(sys.argv) == 3 else None)
