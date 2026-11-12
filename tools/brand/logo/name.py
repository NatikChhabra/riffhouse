"""The Riffhouse script, tuned by hand: letters pulled together until the strokes weld, a baseline that climbs, one swash."""
import math, os
from outline import *
from shapely import affinity
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "..", ".."))  # the repo
YT = _ROOT + "/resources/fonts/Yellowtail-Regular.ttf"
HERE = os.path.dirname(os.path.abspath(__file__))
DAFOE = os.path.join(HERE, "..", "MrDafoe-Regular.ttf")

def script(font=YT, kern=None, rise=4.0, swash=True, weight=0.028, tail="pen"):
    kern = kern if kern is not None else {0: -36, 1: 4, 2: 6, 3: 4, 4: 8, 5: 8, 6: 2, 7: -18}
    s, adv = word(font, "Riffhouse", kern=kern)
    s = s.buffer(weight, quad_segs=8)                      # a touch more ink, so thin joins survive being cast
    x0, y0, x1, y1 = s.bounds
    parts = [s]
    if swash and tail == "pen":
        # out of the last letter, round and back under the whole name, thinning as the pen lifts
        line = bez([(x1 - 0.30, 0.03), (x1 + 0.30, -0.02), (x1 + 0.42, -0.58), (x1 - 0.60, -0.60),
                    (x1 - 1.9, -0.62), (x0 + 2.4, -0.60), (x0 + 1.0, -0.60),
                    (x0 + 0.6, -0.60), (x0 + 0.2, -0.62), (x0 - 0.15, -0.68)], 60)
        w = lambda t: 0.095 + 0.06 * math.sin(math.pi * min(1.0, t * 1.08)) ** 0.8 * (1 - 0.35 * t)
        parts.append(stroke(line, w))
    s = unary_union(parts)
    s = s.buffer(0.012, quad_segs=10).buffer(-0.012, quad_segs=10)   # fill the tight inside corners, as a mould would
    s = soften(s, 0.012)
    s = affinity.rotate(s, rise, origin=(0, 0))
    return s

if __name__ == "__main__":
    import sys
    from PIL import Image, ImageDraw
    def draw(shape, path, k=220, pad=0.4):
        x0, y0, x1, y1 = shape.bounds
        im = Image.new("RGB", (int((x1-x0+2*pad)*k), int((y1-y0+2*pad)*k)), (14,14,14)); d = ImageDraw.Draw(im)
        T = lambda p: ((p[0]-x0+pad)*k, (y1-p[1]+pad)*k)
        for p in polys(shape):
            d.polygon([T(q) for q in p.exterior.coords], fill=(236,228,207))
            for h in p.interiors: d.polygon([T(q) for q in h.coords], fill=(14,14,14))
        im.save(path)
    s = script(); print(len(polys(s)), s.bounds); draw(s, os.path.join(HERE, "out/name-yt.png"))
    s = script(DAFOE, kern={}, weight=0.0); print(len(polys(s)), s.bounds); draw(s, os.path.join(HERE, "out/name-dafoe.png"))
