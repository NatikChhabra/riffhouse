"""The Riffhouse logo as hardware, photographed in the gear room (tools/gear3d/common.py).
  python mk.py <job> <out.png> [samples] [px_per_cm]
"""
import math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE); sys.path.insert(0, _ROOT + "/tools/gear3d")
import bpy
import common as c
from mathutils import Vector
import name as nm
from outline import polys, bez, stroke, word, soften
from shapely import affinity
from shapely.ops import unary_union
import os as _os
_ROOT = _os.path.abspath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "..", ".."))  # the repo

AMBER = c.rgb("ffa31a")
CAP = 3.2            # cap height of the script in cm

def scene(w, h, px, samples):
    sc = c.new_scene(round(w * px), round(h * px), samples)
    cam = bpy.data.cameras.new("cam"); cam.type = "ORTHO"; cam.ortho_scale = max(w, h)
    ob = bpy.data.objects.new("cam", cam); sc.collection.objects.link(ob); ob.location = (0, 0, 60); sc.camera = ob
    c.room(sc); return sc

def bake(ob):
    bpy.context.view_layer.update()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(bpy.context.evaluated_depsgraph_get()))
    me.transform(ob.matrix_world); n = ob.name; bpy.data.objects.remove(ob)
    out = bpy.data.objects.new(n, me); bpy.context.scene.collection.objects.link(out); return out

def solid(shape, k, depth, bevel, mat, z=0.0, at=(0, 0), sharp=40, name="solid", res=5):
    """A flat outline (shapely) cast as a solid `depth` thick with rolled edges, standing on z."""
    if bevel > 0:      # pull the outline in by the roll, then let the roll grow it back: no creases at tight corners
        shape = shape.buffer(-bevel / k, quad_segs=10)
    shape = shape.simplify(0.0012)
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "2D"; cu.fill_mode = "BOTH"
    for p in polys(shape):
        for ring in [p.exterior] + list(p.interiors):
            pts = list(ring.coords)[:-1]
            sp = cu.splines.new("POLY"); sp.points.add(len(pts) - 1)
            for q, (x, y) in zip(sp.points, pts): q.co = (x * k, y * k, 0, 1)
            sp.use_cyclic_u = True
    cu.extrude = max(0.0, depth / 2 - bevel); cu.bevel_depth = bevel; cu.bevel_resolution = res
    ob = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(ob)
    ob.location = (at[0], at[1], z + depth / 2)
    ob = bake(ob)
    if mat is not None: ob.data.materials.append(mat)
    for p in ob.data.polygons: p.use_smooth = True
    ob.data.set_sharp_from_angle(angle=math.radians(sharp))
    return ob

def tube(points, radius, mat, radii=None, name="tube"):
    cu = bpy.data.curves.new(name, "CURVE"); cu.dimensions = "3D"; cu.bevel_depth = radius; cu.bevel_resolution = 8; cu.use_fill_caps = True
    cu.resolution_u = 24
    sp = cu.splines.new("BEZIER"); sp.bezier_points.add(len(points) - 1)
    for i, (bp, p) in enumerate(zip(sp.bezier_points, points)):
        bp.co = p; bp.handle_left_type = bp.handle_right_type = "AUTO"; bp.radius = radii[i] if radii else 1.0
    ob = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(ob); ob = bake(ob)
    ob.data.materials.append(mat)
    for p in ob.data.polygons: p.use_smooth = True
    return ob

# ------------------------------------------------------------------ materials
def tolex():
    m = c.principled("tolex", (0.0045, 0.0042, 0.0042), rough=0.42, coat=0.25, coat_rough=0.25)
    n = c.Nodes(m)
    grain = n.voronoi(13.0, "SMOOTH_F1")
    fine = n.noise(70.0, 3.0)
    n.link(n.bump(n.math("ADD", grain, n.math("MULTIPLY", fine, 0.2)), 1.0, 0.035), n.bsdf.inputs["Normal"])
    return m

def grille(warp="f3d9a0", dark="94733a"):
    m = c.principled("grille", c.rgb(warp), rough=0.9)
    n = c.Nodes(m)
    wx = n.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction="X")
    wy = n.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction="Y")
    for w, s in ((wx, 6.0), (wy, 6.0)):
        w.inputs["Scale"].default_value = s; w.inputs["Distortion"].default_value = 0.2
        n.link(n.coord.outputs["Object"], w.inputs["Vector"])
    weave = n.math("MULTIPLY", wx.outputs["Fac"], wy.outputs["Fac"])
    flecks = n.noise(3.0, 2.0)
    col = n.mix(weave, c.rgb(dark), n.mix(flecks, c.rgb(warp), tuple(v * 0.7 for v in c.rgb(warp))))
    n.link(col, n.bsdf.inputs["Base Color"])
    n.link(n.bump(weave, 0.9, 0.04), n.bsdf.inputs["Normal"])
    return m

def ivory():
    m = c.principled("ivory", c.rgb("efe2bf"), rough=0.28, coat=0.7, coat_rough=0.1)
    n = c.Nodes(m)     # aged: a little yellower in patches
    n.link(n.mix(n.noise(0.5, 3.0), c.rgb("f1e6c6"), c.rgb("e6d3a4")), n.bsdf.inputs["Base Color"])
    return m

def brass():
    m = c.principled("brass", c.rgb("d0a550"), metallic=1.0, rough=0.26)
    n = c.Nodes(m)
    g = n.noise(60.0, 3.0, stretch=(14.0, 0.02, 14.0))
    n.link(n.bump(g, 0.06, 0.006), n.bsdf.inputs["Normal"])
    n.link(n.math("MULTIPLY_ADD", g, 0.12, 0.2), n.bsdf.inputs["Roughness"])
    return m

def glow(strength=30.0, col=AMBER):
    return c.principled("glow", col, emission=col, strength=strength)

def cream_paint():
    return c.principled("cream", c.rgb("efe6cf"), rough=0.45, coat=0.2, coat_rough=0.2)

# ------------------------------------------------------------------ parts
def screw(x, y, z, r=0.3, turn=0.6, mat=None):
    """A pan-head screw with a slot, the kind that holds a back plate or a nameplate on."""
    mat = mat or c.nickel()
    head = c.lathe("screw", [(r, 0.0), (r, 0.05), (r * 0.94, r * 0.3), (r * 0.7, r * 0.52), (0.0001, r * 0.6)], mat, seg=64, at=(x, y, z))
    slot = c.box("slot", -r * 1.2, r * 1.2, -r * 0.13, r * 0.13, r * 0.36, r, None)
    slot.location = (x, y, z); slot.rotation_euler[2] = turn
    c.cut(head, slot)
    return head

def rivet(x, y, z, r=0.3, mat=None):
    return c.lathe("rivet", [(r, 0.0), (r, 0.05), (r * 0.93, r * 0.32), (r * 0.62, r * 0.56), (0.0001, r * 0.64)], mat or c.nickel(), seg=64, at=(x, y, z))

def jewel(x, y, z, r=0.42, col=None, lit=1.3):
    col = col or c.rgb("f07a0c")
    """An amp's pilot light: a faceted glass jewel in a chrome bezel, lit from inside."""
    c.lathe("bezel", [(r * 1.5, 0), (r * 1.5, r * 0.28), (r * 1.34, r * 0.6), (r * 1.1, r * 0.72), (r * 1.02, r * 0.4)], c.chrome(), seg=64, at=(x, y, z))
    lens = c.principled("jewel", tuple(v * 0.6 for v in col), rough=0.06, coat=1.0, coat_rough=0.03, emission=col, strength=lit)
    ob = c.lathe("lens", [(r * 1.02, r * 0.2), (r * 1.02, r * 0.5), (r * 0.8, r * 0.95), (r * 0.42, r * 1.22), (0.0001, r * 1.3)], lens, seg=12, at=(x, y, z))
    for p in ob.data.polygons: p.use_smooth = False
    return ob

def plug(tip, direction, k=1.0, z=0.0, cable_r=0.26):
    """A quarter-inch guitar plug lying on the surface: tip at `tip` (x, y), pointing along `direction` (radians)."""
    R = 0.3175
    holder = bpy.data.objects.new("plug", None); bpy.context.scene.collection.objects.link(holder)
    def part(ob): ob.parent = holder; return ob
    ni = c.principled("plugmetal", (0.86, 0.85, 0.82), metallic=1.0, rough=0.16)
    part(c.lathe("tip", [(0.0001, 0.0), (0.16, 0.03), (0.26, 0.16), (0.30, 0.34), (0.27, 0.5), (0.2, 0.62), (0.2, 0.72), (R, 0.84), (R, 0.98)], ni, seg=64))
    part(c.lathe("ring", [(R * 0.97, 0.98), (R * 0.97, 1.1)], c.black(), seg=64))
    part(c.lathe("sleeve", [(R, 1.1), (R, 3.15)], ni, seg=64))
    part(c.lathe("collar", [(0.5, 3.15), (0.62, 3.22), (0.62, 3.5), (0.58, 3.56)], ni, seg=96))
    part(c.lathe("barrel", [(0.58, 3.5), (0.6, 3.6), (0.6, 6.0), (0.56, 6.1), (0.5, 6.14)], ni, seg=240, flutes=60, depth=0.035, flute_rings=(1, 2)))
    part(c.lathe("relief", [(0.44, 6.1), (0.44, 6.35), (0.36, 6.9), (0.3, 7.4), (cable_r * 1.05, 7.5)], c.rubber(), seg=64))
    # lathe axis is +z with the tip at z=0: lay it so the body runs opposite to `direction`
    holder.rotation_euler = (0, -math.pi / 2, direction)
    holder.scale = (k, k, k)
    holder.location = (tip[0], tip[1], z + 0.62 * k)
    back = (tip[0] - 7.5 * k * math.cos(direction), tip[1] - 7.5 * k * math.sin(direction), z + 0.62 * k)
    return holder, back

# ------------------------------------------------------------------ the name
def name_solid(mat, depth=0.5, bevel=0.11, z=0.0, at=(0, 0), font=None, swash=True, centre=True, **kw):
    s = nm.script(font or nm.YT, swash=swash, **kw)
    x0, y0, x1, y1 = s.bounds
    if centre: s = affinity.translate(s, -(x0 + x1) / 2, -(y0 + y1) / 2)
    ob = solid(s, CAP, depth, bevel, mat, z=z, at=at, name="name")
    return ob, s

def letter(ch, font, cap_cm, mat, depth=0.5, bevel=0.12, z=0.0, at=(0, 0), rise=4.0, weight=0.012, extra=None):
    s, adv = word(font, ch); s = s.buffer(weight)
    if extra is not None: s = unary_union([s, extra(s)])
    s = s.buffer(0.012, quad_segs=10).buffer(-0.012, quad_segs=10); s = soften(s, 0.012); s = affinity.rotate(s, rise, origin=(0, 0))
    x0, y0, x1, y1 = s.bounds; s = affinity.translate(s, -(x0 + x1) / 2, -(y0 + y1) / 2)
    return solid(s, cap_cm, depth, bevel, mat, z=z, at=at, name="letter"), s

# ------------------------------------------------------------------ jobs
def word_tolex(px, samples, cable=False):
    """Cream script on black tolex, the badge on the front of an old combo."""
    scene(24, 10.5, px, samples)
    c.box("cover", -30, 30, -20, 20, -1.0, 0.0, tolex())
    name_solid(ivory(), at=(0, 0.2))

def word_grille(px, samples):
    scene(24, 10.5, px, samples)
    c.box("cloth", -30, 30, -20, 20, -1.0, 0.0, grille())
    name_solid(brass(), depth=0.42, bevel=0.1, at=(0, 0.2))

def word_clear(px, samples, mat="ivory"):
    scene(24, 10.5, px, samples)
    name_solid({"ivory": ivory, "brass": brass, "chrome": c.chrome}[mat](), at=(0, 0.2))

def crinkle():
    m = c.principled("crinkle", (0.003, 0.003, 0.0034), rough=0.6)
    m.node_tree.nodes["Principled BSDF"].inputs["Specular IOR Level"].default_value = 0.22
    n = c.Nodes(m)
    w = n.voronoi(30.0, "SMOOTH_F1")
    n.link(n.bump(n.math("ADD", w, n.math("MULTIPLY", n.noise(110.0, 3.0), 0.4)), 0.6, 0.02), n.bsdf.inputs["Normal"])
    return m

def plate_base(w, h, mat, z1=0.4, r=0.9, rivets=True, inset=0.75):
    from shapely.geometry import box as sbox
    solid(sbox(-w / 2 + r, -h / 2 + r, w / 2 - r, h / 2 - r).buffer(r, quad_segs=24), 1.0, z1, 0.08, mat, z=0.0, name="plate")
    if rivets:
        for sx in (-1, 1):
            for sy in (-1, 1):
                rivet(sx * (w / 2 - inset), sy * (h / 2 - inset), z1, r=0.24)

def word_pedal(px, samples):
    """The name printed in cream on a stomp box's orange enamel, screws at the corners."""
    scene(24, 10.5, px, samples)
    c.box("body", -11.6, 11.6, -4.9, 4.9, 0.0, 2.2, c.finish_material(0, "ff7a08"), corner=1.2, edge=0.3, taper=(0.985, 0.965), corner_segments=16)
    for sx in (-1, 1):
        for sy in (-1, 1):
            screw(sx * 10.5, sy * 3.8, 2.2, r=0.3, turn=0.5 + sx * 0.9 + sy * 0.4)
    name_solid(cream_paint(), depth=0.07, bevel=0.03, z=2.2, at=(0, 0.1))

def word_backlit(px, samples):
    """The name cut through a riveted steel plate, the amp's lamp glowing behind it."""
    import icons
    from shapely.geometry import box as sbox
    scene(24, 10.5, px, samples)
    s = nm.script(); x0, y0, x1, y1 = s.bounds
    s = affinity.translate(s, -(x0 + x1) / 2, -(y0 + y1) / 2 + 0.03)
    s = affinity.scale(s, CAP, CAP, origin=(0, 0))
    r = 0.9; plate = sbox(-11.6 + r, -4.9 + r, 11.6 - r, 4.9 - r).buffer(r, quad_segs=24)
    islands = unary_union([__import__("shapely").geometry.Polygon(h) for p in polys(s) for h in p.interiors])
    solid(plate.difference(s), 1.0, 0.36, 0.06, icons.brushed(), z=0.0, name="plate", sharp=35)
    for sx in (-1, 1):
        for sy in (-1, 1):
            rivet(sx * 10.6, sy * 3.9, 0.36, r=0.26)
    m = icons.lamp(strength=1.5, spread=0.07).data.materials[0]; bpy.data.objects.remove(bpy.data.objects['lamp'])
    solid(plate.buffer(-0.3), 1.0, 0.1, 0.0, m, z=-0.6, name='lampface')

def word_plug(px, samples, plate=True):
    """Cream script on a black crinkle plate; the stroke under the name is a guitar cable, ending in its plug."""
    scene(24, 10.5, px, samples)
    if plate: plate_base(23.2, 9.8, crinkle())
    z = 0.4
    ob, s = name_solid(ivory(), swash=False, z=z, at=(0.3, 1.15))
    x0, y0, x1, y1 = [v * CAP for v in s.bounds]; x0 += 0.3; x1 += 0.3; y0 += 1.15; y1 += 1.15
    k = 0.8
    tip = (x0 - 1.0, y0 - 0.72)
    holder, back = plug(tip, math.pi + math.radians(4), k=k, z=z)
    cab = c.principled("cable", c.rgb("e9dcb8"), rough=0.4, coat=0.3, coat_rough=0.2)
    rr = 0.26 * k; zc = back[2]
    # out of the last letter, round, and back under the name into the plug
    ex, ey = x1 - 0.55, y0 + 2.05
    pts = [(ex - 0.5, ey + 0.12, z + 0.22), (ex, ey, z + 0.24), (x1 + 0.35, ey - 0.35, zc), (x1 + 0.55, y0 + 0.55, zc), (x1 - 0.6, back[1] + 0.42, zc),
           ((x1 + back[0]) / 2, back[1] + 0.32, zc), (back[0] + 1.2, back[1] + 0.08, zc), (back[0] - 0.1, back[1], zc)]
    tube(pts, rr, cab)

JOBS = {"word-plug-clear": lambda px, samples: word_plug(px, samples, plate=False), "word-pedal": word_pedal, "word-backlit": word_backlit, "word-plug": word_plug, "word-tolex": word_tolex, "word-grille": word_grille, "word-ivory": word_clear}

if __name__ == "__main__":
    job, out = sys.argv[1], sys.argv[2]
    samples = int(sys.argv[3]) if len(sys.argv) > 3 else 24
    px = float(sys.argv[4]) if len(sys.argv) > 4 else 50
    if "icons" not in sys.modules:
        try:
            import icons; JOBS.update(icons.JOBS)
        except ImportError: pass
    JOBS[job](px, samples)
    bpy.context.scene.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("wrote", out)
