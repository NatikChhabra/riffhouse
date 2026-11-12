import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import flat2 as f
from shapely import affinity
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "..", ".."))  # the repo
OUT = sys.argv[1]; M = os.path.join(os.path.dirname(os.path.abspath(__file__)), "master2")
os.makedirs(OUT, exist_ok=True)
F = _ROOT + "/resources/fonts/"
BG = (11, 11, 11, 255)
def fit(im, w): return im.resize((w, round(im.height * w / im.width)), Image.LANCZOS)
def trim(im, pad=0):
    b = im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    return im.crop((max(0, b[0] - pad), max(0, b[1] - pad), min(im.width, b[2] + pad), min(im.height, b[3] + pad)))
icon = Image.open(f"{M}/icon-1024.png").convert("RGBA")
clear = trim(Image.open(f"{M}/wordmark-clear.png").convert("RGBA"), 12)
tolex = Image.open(f"{M}/wordmark-tolex.png").convert("RGBA")
# the mark
icon.save(f"{OUT}/mark-1024.png"); fit(icon, 512).save(f"{OUT}/mark-512.png"); fit(icon, 256).save(f"{OUT}/mark-256.png")
small = lambda sz: f.raster(f.mark(True), 100, 100, sz)
for sz in (16, 24, 32, 48): small(sz).save(f"{OUT}/mark-small-{sz}.png")
open(f"{OUT}/mark-small.svg", "w").write(f.svg(f.mark(True), 100, 100)); open(f"{OUT}/mark-flat.svg", "w").write(f.svg(f.mark(False), 100, 100))
f.raster(f.mark(False), 100, 100, 1024).save(f"{OUT}/mark-flat-1024.png")
# the wordmark
clear.save(f"{OUT}/wordmark-3d.png"); tolex.convert("RGB").save(f"{OUT}/wordmark-on-tolex.png")
wm, w, h = f.wordmark(); wms = affinity.scale(wm, 100, 100, origin=(0, 0))
open(f"{OUT}/wordmark-flat.svg", "w").write(f.svg([(f.CREAM, wms)], w * 100, h * 100))
open(f"{OUT}/wordmark-one-colour.svg", "w").write(f.svg([(f.CREAM, wms)], w * 100, h * 100, one_colour=True))
f.raster([(f.CREAM, wm)], w, h, 1600).save(f"{OUT}/wordmark-flat-cream.png"); f.raster([("#141312", wm)], w, h, 1600).save(f"{OUT}/wordmark-flat-ink.png")
# app icon: the photographed mark from 48 up, the drawn small mark below
sizes = [256, 128, 64, 48, 32, 24, 16]
frames = [fit(icon, s) if s >= 48 else small(s) for s in sizes]
frames[0].save(f"{OUT}/riffhouse.ico", format="ICO", append_images=frames[1:], sizes=[(s, s) for s in sizes])
fav = [small(s) for s in (48, 32, 16)]
fav[0].save(f"{OUT}/favicon.ico", format="ICO", append_images=fav[1:], sizes=[(48, 48), (32, 32), (16, 16)])
small(32).save(f"{OUT}/favicon-32.png")
def on_bg(size, frac, bg=BG):
    c = Image.new("RGBA", (size, size), bg); i = fit(icon, round(size * frac)); c.alpha_composite(i, ((size - i.width) // 2, (size - i.height) // 2)); return c
on_bg(180, 0.92).convert("RGB").save(f"{OUT}/apple-touch-icon-180.png")
on_bg(1080, 0.66).convert("RGB").save(f"{OUT}/profile-picture-1080.png")
# lockup: mark left of the name, clear background
H = 520; i = fit(icon, H); wd = clear.resize((round(clear.width * (H * 0.86) / clear.height), round(H * 0.86)), Image.LANCZOS)
lock = Image.new("RGBA", (i.width + 70 + wd.width, H), (0, 0, 0, 0)); lock.alpha_composite(i, (0, 0)); lock.alpha_composite(wd, (i.width + 70, (H - wd.height) // 2)); lock.save(f"{OUT}/lockup-transparent.png")
# social card 1200 x 630: the badge on its tolex, the line under it
card = tolex.resize((1200, round(tolex.height * 1200 / tolex.width)), Image.LANCZOS)
canvas = Image.new("RGBA", (1200, 630), BG); canvas.alpha_composite(card.crop((0, 0, 1200, min(card.height, 630))), (0, 0))
shade = Image.new("L", (1200, 630), 0); ImageDraw.Draw(shade).rectangle([0, 470, 1200, 630], fill=235); shade = shade.filter(ImageFilter.GaussianBlur(40))
canvas.paste(Image.new("RGBA", (1200, 630), (8, 8, 8, 255)), (0, 0), shade)
d = ImageDraw.Draw(canvas); big = ImageFont.truetype(F + "BigShoulders-ExtraBold.ttf", 54); sm = ImageFont.truetype(F + "InstrumentSans-Regular.ttf", 28)
d.text((64, 520), "MUSIC SHOULD BE FREE", font=big, fill=(240, 138, 28)); d.text((66, 582), "Free guitar amp and pedalboard for Windows", font=sm, fill=(214, 206, 188))
ii = fit(icon, 112); canvas.alpha_composite(ii, (1200 - 112 - 56, 630 - 112 - 34))
canvas.convert("RGB").save(f"{OUT}/social-card-1200x630.png")
open(f"{OUT}/README.txt", "w").write("""Riffhouse logo, final (Combo badge), 2026-11-12

mark-1024.png, mark-512.png, mark-256.png   the amp-face icon, photographed in the gear room, clear background
mark-small.svg, mark-small-16/24/32/48.png  the drawn mark for 16 to 32 px (taskbar, tab, top bar)
mark-flat.svg, mark-flat-1024.png           the full mark drawn flat, for print or one-pass uploads
wordmark-3d.png                             the cream script badge alone, clear background
wordmark-on-tolex.png                       the badge on black tolex
wordmark-flat.svg, wordmark-flat-cream.png  flat script, cream, for dark grounds and small sizes
wordmark-one-colour.svg                     flat script using currentColor
wordmark-flat-ink.png                       flat script in near-black, for the amber site hero
riffhouse.ico                               app and installer icon, 256 to 16 (48 and up photographed, below drawn)
favicon.ico, favicon-32.png, apple-touch-icon-180.png
profile-picture-1080.png                    square, opaque, mark inside the round crop
lockup-transparent.png                      mark and name side by side
social-card-1200x630.png                    link preview

Colours: cream #efe2bf, frame #171615, cloth #86693a, lamp #f08a1c, page #0b0b0b.
Script: Yellowtail (OFL), strokes thickened and welded, baseline rising 4 degrees, one swash.
Source: ../round2/source (mk.py, icons.py, name.py, flat2.py, assemble.py). Needs bpy, shapely, fonttools, pillow.
""")
print("ok")
