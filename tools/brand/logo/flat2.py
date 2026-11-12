"""Flat versions of the chosen logo (Combo badge): SVG + PNG from the same outlines as the 3D build."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw
import name as nm
from outline import polys, word, soften, svg_path
from shapely import affinity
from shapely.geometry import box as sbox, Point
from shapely.ops import unary_union
CREAM, FRAME, CLOTH, SHADE, LAMP = "#efe2bf", "#171615", "#86693a", "#4d3b1e", "#f08a1c"

def rsq(c0, half, r):
    return sbox(c0 - half + r, c0 - half + r, c0 + half - r, c0 + half - r).buffer(r, quad_segs=24)

def letter(cap, cx, cy, weight=0.03):
    s, _ = word(nm.YT, "R"); s = s.buffer(weight); s = soften(s, 0.012); s = affinity.rotate(s, 4, origin=(0, 0))
    x0, y0, x1, y1 = s.bounds
    return affinity.translate(affinity.scale(affinity.translate(s, -(x0 + x1) / 2, -(y0 + y1) / 2), cap, cap, origin=(0, 0)), cx, cy)

def mark(small=False):
    """On a 100 square, ordered back to front: [(colour, shape)]."""
    if small:      # 16 to 32 px: frame, cloth, the R and the light; no piping, no shadow
        R = letter(52, 44.5, 56.5, 0.052)
        return [(FRAME, rsq(50, 50, 20)), (CLOTH, rsq(50, 39, 10)), (CREAM, R), (LAMP, Point(78.5, 21.5).buffer(7.5))]
    R = letter(49, 48.5, 52)
    pipe = rsq(50, 37.5, 8).difference(rsq(50, 35, 6))
    return [(FRAME, rsq(50, 50, 13)), (CLOTH, rsq(50, 36, 7)), (CREAM, pipe), (SHADE, affinity.translate(R, 1.6, -2.2)), (CREAM, R),
            ("#fff1d2", Point(75.5, 24.5).buffer(6.2)), (LAMP, Point(75.5, 24.5).buffer(4.6))]

def wordmark(pad=0.2):
    s = nm.script(rise=4); x0, y0, x1, y1 = s.bounds
    return affinity.translate(s, -x0 + pad, -y0 + pad), (x1 - x0) + 2 * pad, (y1 - y0) + 2 * pad

def svg(layers, w, h, one_colour=False):
    body = "".join(f'<path fill="{"currentColor" if one_colour else col}" fill-rule="evenodd" d="{svg_path(g, h)}"/>' for col, g in layers)
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:g} {h:g}">{body}</svg>'

def raster(layers, w, h, px, k=4, bg=(0, 0, 0, 0)):
    s = px * k / w
    im = Image.new("RGBA", (round(w * s), round(h * s)), bg)
    for col, g in layers:
        layer = Image.new("L", im.size, 0); ld = ImageDraw.Draw(layer)
        for p in polys(g):
            ld.polygon([(x * s, (h - y) * s) for x, y in p.exterior.coords], fill=255)
            for hh in p.interiors: ld.polygon([(x * s, (h - y) * s) for x, y in hh.coords], fill=0)
        im.paste(Image.new("RGBA", im.size, col), (0, 0), layer)
    return im.resize((round(px), round(px * h / w)), Image.LANCZOS)

if __name__ == "__main__":
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    wm, w, h = wordmark(); sc = 100.0; wms = affinity.scale(wm, sc, sc, origin=(0, 0))
    open(f"{out}/wordmark-flat.svg", "w").write(svg([(CREAM, wms)], w * sc, h * sc))
    open(f"{out}/wordmark-one-colour.svg", "w").write(svg([(CREAM, wms)], w * sc, h * sc, one_colour=True))
    raster([(CREAM, wm)], w, h, 1600).save(f"{out}/wordmark-flat.png")
    raster([("#141312", wm)], w, h, 1600).save(f"{out}/wordmark-flat-dark-ink.png")
    for nme, small in (("mark-flat", False), ("mark-small", True)):
        open(f"{out}/{nme}.svg", "w").write(svg(mark(small), 100, 100))
        raster(mark(small), 100, 100, 512).save(f"{out}/{nme}-512.png")
    sheet = Image.new("RGBA", (1500, 560), (11, 11, 11, 255))
    sheet.alpha_composite(raster([(CREAM, wm)], w, h, 700), (30, 30))
    sheet.alpha_composite(raster([(CREAM, wm)], w, h, 150), (30, 420)); sheet.alpha_composite(raster([(CREAM, wm)], w, h, 90), (220, 440))
    light = Image.new("RGBA", (300, 130), (240, 138, 28, 255)); light.alpha_composite(raster([("#141312", wm)], w, h, 240), (30, 12)); sheet.alpha_composite(light, (400, 400))
    for small in (False, True):
        xx = 780
        for sz in (256, 64, 48, 32, 24, 16):
            sheet.alpha_composite(raster(mark(small), 100, 100, sz), (xx, 30 + (280 if small else 0))); xx += sz + 16
    white = Image.new("RGBA", (200, 60), (245, 245, 245, 255))
    for i, sz in enumerate((32, 24, 16)): white.alpha_composite(raster(mark(True), 100, 100, sz), (14 + i * 50, 12))
    sheet.alpha_composite(white, (1150, 480))
    sheet.convert("RGB").save(f"{out}/flat-proof.png")
