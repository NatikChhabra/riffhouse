"""The name as one merged outline: glyphs from the font, welded, plus a swash. Units: 1.0 = cap height."""
import math
from fontTools.ttLib import TTFont
from fontTools.pens.basePen import BasePen
from shapely.geometry import Polygon, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity

class Flat(BasePen):
    def __init__(self, gs, steps=14):
        super().__init__(gs); self.contours = []; self.steps = steps
    def _moveTo(self, p): self.contours.append([p])
    def _lineTo(self, p): self.contours[-1].append(p)
    def _curveToOne(self, a, b, c):
        p0 = self.contours[-1][-1]
        for i in range(1, self.steps + 1):
            t = i / self.steps; u = 1 - t
            self.contours[-1].append((u**3*p0[0]+3*u*u*t*a[0]+3*u*t*t*b[0]+t**3*c[0], u**3*p0[1]+3*u*u*t*a[1]+3*u*t*t*b[1]+t**3*c[1]))
    def _qCurveToOne(self, a, c):
        p0 = self.contours[-1][-1]
        for i in range(1, self.steps + 1):
            t = i / self.steps; u = 1 - t
            self.contours[-1].append((u*u*p0[0]+2*u*t*a[0]+t*t*c[0], u*u*p0[1]+2*u*t*a[1]+t*t*c[1]))
    def _closePath(self): pass
    def _endPath(self): pass

def area(c):
    return sum(c[i][0]*c[(i+1)%len(c)][1]-c[(i+1)%len(c)][0]*c[i][1] for i in range(len(c)))/2

def glyph_shape(font, gname):
    gs = font.getGlyphSet(); pen = Flat(gs); gs[gname].draw(pen)
    cs = [c for c in pen.contours if len(c) > 2]
    if not cs: return None
    big = max(cs, key=lambda c: abs(area(c))); sign = 1 if area(big) > 0 else -1
    outer = unary_union([Polygon(c).buffer(0) for c in cs if area(c)*sign > 0])
    holes = [Polygon(c).buffer(0) for c in cs if area(c)*sign < 0]
    return outer.difference(unary_union(holes)) if holes else outer

def word(path, text, kern=None, track=0.0):
    """Returns (shape, advance) scaled so the cap height is 1. kern: {index: extra advance after that letter, in em/1000}."""
    f = TTFont(path); cmap = f.getBestCmap(); upm = f["head"].unitsPerEm
    cap = getattr(f["OS/2"], "sCapHeight", 0) or upm * 0.7
    x, parts = 0.0, []
    for i, ch in enumerate(text):
        g = cmap[ord(ch)]; s = glyph_shape(f, g)
        if s is not None: parts.append(affinity.translate(s, x, 0))
        x += f["hmtx"][g][0] + track * upm / 1000 + (kern or {}).get(i, 0) * upm / 1000
    s = unary_union(parts)
    return affinity.scale(s, 1/cap, 1/cap, origin=(0, 0)), x / cap

def bez(pts, n=40):
    """A chain of cubic segments given as [p0, c1, c2, p1, c1, c2, p2 ...] flattened."""
    out = []
    for k in range(0, len(pts) - 1, 3):
        p0, a, b, c = pts[k:k+4]
        for i in range(n):
            t = i / n; u = 1 - t
            out.append((u**3*p0[0]+3*u*u*t*a[0]+3*u*t*t*b[0]+t**3*c[0], u**3*p0[1]+3*u*u*t*a[1]+3*u*t*t*b[1]+t**3*c[1]))
    out.append(pts[-1]); return out

def stroke(line, width):
    """A pen stroke along `line`; width(t) for t in 0..1 gives the full width there."""
    L, R = [], []; n = len(line)
    for i, p in enumerate(line):
        a = line[max(0, i-1)]; b = line[min(n-1, i+1)]
        dx, dy = b[0]-a[0], b[1]-a[1]; d = math.hypot(dx, dy) or 1
        w = width(i/(n-1))/2
        L.append((p[0]-dy/d*w, p[1]+dx/d*w)); R.append((p[0]+dy/d*w, p[1]-dx/d*w))
    return Polygon(L + R[::-1]).buffer(0)

def polys(shape):
    return list(shape.geoms) if isinstance(shape, MultiPolygon) else [shape]

def soften(shape, r):
    return shape.buffer(r, quad_segs=8).buffer(-2*r, quad_segs=8).buffer(r, quad_segs=8)

def svg_path(shape, flip_h=None):
    d = []
    for p in polys(shape):
        for ring in [p.exterior] + list(p.interiors):
            c = list(ring.coords)
            d.append("M" + " L".join("%.3f %.3f" % (x, (flip_h - y) if flip_h is not None else y) for x, y in c[:-1]) + "Z")
    return " ".join(d)
