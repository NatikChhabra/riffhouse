"""Make the two pictures the first screen's pedal is built from.

The gear cutouts carry their own soft shadow. The first screen draws the pedal as a solid
object that turns, so it needs the face without that shadow (the page casts its own), and a
darker copy of the same shape that is stacked behind the face to give the pedal its sides.

    python3 tools/site/hero_pedal.py            # reads site/img/gear/drive-5.webp
"""
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance

SITE = Path(__file__).resolve().parents[2] / "site" / "img"


def main(source="gear/drive-5.webp"):
    cut = np.array(Image.open(SITE / source).convert("RGBA"))
    # the shadow is the only part that is both black and see-through
    shadow = (cut[..., :3].max(axis=2) < 14) & (cut[..., 3] < 252)
    cut[shadow, 3] = 0
    face = Image.fromarray(cut)
    face.save(SITE / "hero-pedal.webp", quality=88, method=6)

    side = face.resize((face.width // 2, face.height // 2), Image.LANCZOS)
    alpha = side.getchannel("A")
    side = ImageEnhance.Brightness(side.convert("RGB")).enhance(0.7).convert("RGBA")
    side.putalpha(alpha)
    side.save(SITE / "hero-side.webp", quality=60, method=6)
    for name in ("hero-pedal.webp", "hero-side.webp"):
        print(name, (SITE / name).stat().st_size // 1024, "KB")


if __name__ == "__main__":
    main()
