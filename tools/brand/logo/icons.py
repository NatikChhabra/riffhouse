"""The small mark, four ways, each a piece of hardware."""
import math
import bpy
import common as c
import mk
import name as nm
from outline import word, bez, polys, soften
from shapely.geometry import Polygon, box as sbox, Point
from shapely import affinity
from shapely.ops import unary_union
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "..", ".."))  # the repo

STENCIL = _ROOT + "/resources/fonts/BigShoulders-ExtraBold.ttf"

def rsquare(half, r):
    return sbox(-half + r, -half + r, half - r, half - r).buffer(r, quad_segs=24)

def pick(k=1.0):
    segs = [((0, 4.6), (2.6, 4.6), (4.6, 3.6), (4.6, 1.4)), ((4.6, 1.4), (4.6, -1.4), (1.2, -4.6), (0, -4.7)),
            ((0, -4.7), (-1.2, -4.6), (-4.6, -1.4), (-4.6, 1.4)), ((-4.6, 1.4), (-4.6, 3.6), (-2.6, 4.6), (0, 4.6))]
    pts = []
    for s in segs: pts += bez(list(s), 48)[:-1]
    return affinity.scale(Polygon(pts), k, k, origin=(0, 0))

def stencil(text="RH", cap=3.6, gap=0.5):
    """Stencil capitals as one outline with the bridges left standing; cap height in cm, centred."""
    cuts = {"R": [(0.46, 0.56, -0.1, 1.1)], "H": [(0.43, 0.57, 0.0, 0.36), (0.43, 0.57, 0.64, 1.0)]}
    out, x = [], 0.0
    for ch in text:
        s, adv = word(STENCIL, ch); x0, y0, x1, y1 = s.bounds
        for a, b, p, q in cuts.get(ch, []):
            s = s.difference(sbox(x0 + (x1 - x0) * a, y0 + (y1 - y0) * p - 0.01, x0 + (x1 - x0) * b, y0 + (y1 - y0) * q + 0.01))
        out.append(affinity.translate(s, x - x0, 0)); x += (x1 - x0) + gap / cap
    s = unary_union(out); x0, y0, x1, y1 = s.bounds
    return affinity.scale(affinity.translate(s, -(x0 + x1) / 2, -(y0 + y1) / 2), cap, cap, origin=(0, 0))

def brushed(hexc="c9c9c9", along="x"):
    m = c.principled("brushed", tuple(min(1.0, v * 1.1) for v in c.rgb(hexc)), metallic=1.0, rough=0.3)
    n = c.Nodes(m)
    st = (0.02, 14.0, 14.0) if along == "x" else (14.0, 0.02, 14.0)
    grain = n.noise(40.0, 3.0, stretch=st)
    n.link(n.bump(grain, 0.1, 0.008), n.bsdf.inputs["Normal"])
    n.link(n.math("MULTIPLY_ADD", grain, 0.18, 0.22), n.bsdf.inputs["Roughness"])
    return m

def lamp(half=4.4, z=-0.7, strength=3.2, spread=0.16):
    """The light behind a cut plate: hot in the middle, falling off, the way a bulb lights a panel."""
    m = c.principled("lamp", mk.AMBER)
    n = c.Nodes(m)
    grad = n.new("ShaderNodeTexGradient", gradient_type="SPHERICAL")
    mp = n.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (spread, spread, spread)
    n.link(n.coord.outputs["Object"], mp.inputs["Vector"]); n.link(mp.outputs["Vector"], grad.inputs["Vector"])
    col = n.ramp(grad.outputs["Fac"], [(0.0, c.rgb("d85a06")), (0.6, c.rgb("f58410")), (1.0, c.rgb("ffae3a"))])
    n.link(col, n.bsdf.inputs["Emission Color"]); n.bsdf.inputs["Emission Strength"].default_value = strength
    n.bsdf.inputs["Base Color"].default_value = (0, 0, 0, 1)
    ob = c.box("lamp", -half, half, -half, half, z - 0.1, z, m)
    return ob

# ------------------------------------------------------------------ icons (11 x 11 cm)
def icon_pick(px, samples, script=False):
    """A's riveted plate, cut as a guitar pick: stencil RH through brushed steel, the pilot light behind it."""
    mk.scene(11, 11, px, samples)
    shape = pick(1.08)
    cutter = affinity.translate(stencil("RH", cap=3.5, gap=0.42), 0, 0.75)
    mk.solid(shape.difference(cutter), 1.0, 0.36, 0.07, brushed(), z=0.0, name="plate", sharp=35)
    for x, y in ((-3.45, 3.55), (3.45, 3.55), (0, -3.95)):
        mk.rivet(x, y, 0.36, r=0.3)
    m = lamp(strength=1.5).data.materials[0]
    bpy.data.objects.remove(bpy.data.objects["lamp"])
    mk.solid(pick(1.0), 1.0, 0.1, 0.0, m, z=-0.6, name="lampface")

def icon_amp(px, samples):
    """The front of a small combo: tolex, piping, grille cloth, the R as its badge, the pilot light on."""
    mk.scene(11, 11, px, samples)
    outer, inner = rsquare(5.1, 1.25), rsquare(3.85, 0.7)
    mk.solid(outer.difference(inner), 1.0, 1.3, 0.3, mk.tolex(), z=0.0, name="cab", sharp=50)
    mk.solid(inner.buffer(0.3), 1.0, 0.2, 0.0, mk.grille(), z=0.55, name="cloth")
    pipe = inner.buffer(0.02).difference(inner.buffer(-0.2))
    mk.solid(pipe, 1.0, 0.24, 0.1, c.principled("piping", c.rgb("e9dfc6"), rough=0.4), z=0.72, name="piping")
    mk.letter("R", nm.YT, 4.6, mk.ivory(), depth=0.5, bevel=0.14, z=0.75, at=(-0.15, 0.25), rise=4)
    mk.jewel(2.75, -2.75, 0.75, r=0.36)

def icon_pedal(px, samples):
    """A stomp box seen from above, in the pilot-light orange: four screws, the R printed in cream, its light on."""
    mk.scene(11, 11, px, samples)
    body = c.box("body", -4.9, 4.9, -4.9, 4.9, 0.0, 2.2, c.finish_material(0, "ff7a08"), corner=1.3, edge=0.3, taper=(0.965, 0.965), corner_segments=16)
    for sx in (-1, 1):
        for sy in (-1, 1):
            mk.screw(sx * 3.7, sy * 3.7, 2.2, r=0.38, turn=0.5 + sx * 0.9 + sy * 0.4)
    mk.letter("R", nm.YT, 4.9, mk.cream_paint(), depth=0.07, bevel=0.03, z=2.2, at=(-0.1, 0.1), rise=4)
    mk.jewel(0.0, 3.6, 2.2, r=0.34)

def icon_plate(px, samples):
    """A's riveted plate in black crinkle, B's R in ivory, and a guitar plug as the stroke under it."""
    mk.scene(11, 11, px, samples)
    mk.solid(rsquare(5.0, 1.2), 1.0, 0.4, 0.08, mk.crinkle(), z=0.0, name="plate")
    for sx in (-1, 1):
        for sy in (-1, 1):
            mk.rivet(sx * 3.95, sy * 3.95, 0.4, r=0.28)
    ob, s = mk.letter("R", nm.YT, 4.1, mk.ivory(), depth=0.5, bevel=0.14, z=0.4, at=(-0.2, 1.05), rise=4)
    k = 0.8
    holder, back = mk.plug((-3.55, -2.8), math.pi + math.radians(4), k=k, z=0.4)
    cab = c.principled("cable", c.rgb("e9dcb8"), rough=0.4, coat=0.3, coat_rough=0.2)
    zc = back[2]
    pts = [(0.75, -0.3, 0.56), (1.3, -0.72, 0.58), (2.6, -0.95, zc), (3.25, -1.8, zc), (2.9, -2.45, zc), (back[0] - 0.1, back[1], zc)]
    mk.tube(pts, 0.26 * k, cab)

JOBS = {"icon-pick": icon_pick, "icon-amp": icon_amp, "icon-pedal": icon_pedal, "icon-plate": icon_plate}
