# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""What every 3D picture of the gear shares: the room, the light, the camera and the materials.

The app lays its gear out flat, in "nominal" pixels. The pictures are taken from where the
player stands: an orthographic camera tilted a little toward the gear, so a pedal shows its
front wall and a knob shows its side. Lengths that run away from the player are built longer
by 1 / cos(tilt), so everything on a top face lands exactly where the app's layout has it.
"""
import math

import bpy  # isort: skip (bmesh and mathutils come with it)
import bmesh
from mathutils import Vector
from mathutils.bvhtree import BVHTree

TILT = math.radians(11.0)
SIN, COS = math.sin(TILT), math.cos(TILT)
VIEW = Vector((0.0, SIN, -COS))            # the way the camera looks
CM = 11.5 / 420.0                          # centimetres per nominal pixel: a pedal is 11.5 cm long


def rgb(hexstr):
    """'2e6a42' as linear RGB."""
    v = int(hexstr[-6:], 16)
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin((v >> 16) & 255), lin((v >> 8) & 255), lin(v & 255))


def luminance(c):
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


# ------------------------------------------------------------------ scene
def new_scene(width_px, height_px, samples=128):
    import addon_utils
    bpy.ops.wm.read_factory_settings(use_empty=True)
    addon_utils.enable("cycles")
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 6
    scene.cycles.seed = 7
    scene.render.film_transparent = True
    scene.render.resolution_x, scene.render.resolution_y = int(width_px), int(height_px)
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.compression = 60
    scene.view_settings.view_transform = "Standard"
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 4
    return scene


class Stage:
    """A picture of `area` (x, y, w, h in nominal pixels) whose top faces stand `top` cm above the floor."""

    def __init__(self, area, top, scale=2.0, samples=128, floor=True):
        self.area, self.top = area, top
        self.scene = new_scene(round(area[2] * scale), round(area[3] * scale), samples)
        self.cx, self.cy = area[0] + area[2] / 2.0, area[1] + area[3] / 2.0
        cam = bpy.data.cameras.new("cam")
        cam.type = "ORTHO"
        cam.ortho_scale = max(area[2], area[3]) * CM
        cam.clip_end = 400
        ob = bpy.data.objects.new("cam", cam)
        self.scene.collection.objects.link(ob)
        aim = Vector((0.0, 0.0, top))
        ob.location = aim - VIEW * 120.0
        ob.rotation_euler = VIEW.to_track_quat("-Z", "Y").to_euler()
        self.scene.camera = ob
        room(self.scene)
        if floor:
            bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, 0))
            f = bpy.context.object
            f.data.materials.append(principled("floor", (0.012, 0.012, 0.013), rough=0.85))   # the dark board the gear stands on
            f.is_shadow_catcher = True

    def at(self, x, y, z=None):
        """The point at height z (default: the top face) that the camera sees at nominal (x, y)."""
        z = self.top if z is None else z
        yy = (self.cy - y) * CM
        return Vector(((x - self.cx) * CM, (yy - (z - self.top) * SIN) / COS, z))

    def length(self, nominal):           # across the picture
        return nominal * CM

    def depth(self, nominal):            # away from the player, on a level face
        return nominal * CM / COS

    def on(self, objects, x, y):
        """Where the camera's ray through nominal (x, y) first meets `objects`: (point, normal)."""
        bpy.context.view_layer.update()
        deps = bpy.context.evaluated_depsgraph_get()
        origin = self.at(x, y, self.top) - VIEW * 80.0
        best = None
        for ob in objects:
            ev = ob.evaluated_get(deps)
            inv = ev.matrix_world.inverted()
            hit, loc, nrm, _ = ev.ray_cast(inv @ origin, (inv.to_3x3() @ VIEW).normalized())
            if hit:
                p = ev.matrix_world @ loc
                n = (ev.matrix_world.to_3x3() @ nrm).normalized()
                if best is None or (p - origin).length < (best[0] - origin).length:
                    best = (p, n)
        return best if best else (self.at(x, y), Vector((0, 0, 1)))

    def render(self, path):
        self.scene.render.filepath = path
        bpy.ops.render.render(write_still=True)


def room(scene):
    """One window up and to the left, a weak far wall, and a little more light toward the back."""
    def area(name, loc, size, energy, color=(1, 1, 1), size_y=None):
        l = bpy.data.lights.new(name, "AREA")
        l.energy, l.size, l.color = energy, size, color
        if size_y:
            l.shape, l.size_y = "RECTANGLE", size_y
        o = bpy.data.objects.new(name, l)
        scene.collection.objects.link(o)
        o.location = loc
        o.rotation_euler = (Vector((0, 0, 1.5)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    # far enough away that a wide amp is lit like a small pedal
    area("key", (-38, 42, 84), 20, 76000, (1.0, 0.96, 0.9))
    area("fill", (60, -48, 64), 96, 11200, (0.9, 0.94, 1.0))
    area("rim", (44, 64, 28), 10, 8000, size_y=56)

    world = bpy.data.worlds.new("room")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord")

    def lobe(direction, lo, hi, gain):
        d = nt.nodes.new("ShaderNodeVectorMath")
        d.operation = "DOT_PRODUCT"
        d.inputs[1].default_value = Vector(direction).normalized()
        nt.links.new(tc.outputs["Generated"], d.inputs[0])
        r = nt.nodes.new("ShaderNodeMapRange")
        r.interpolation_type = "SMOOTHSTEP"
        r.inputs["From Min"].default_value, r.inputs["From Max"].default_value = lo, hi
        r.inputs["To Max"].default_value = gain
        nt.links.new(d.outputs["Value"], r.inputs["Value"])
        return r.outputs["Result"]

    def add(a, b):
        n = nt.nodes.new("ShaderNodeMath")
        n.operation = "ADD"
        for i, v in enumerate((a, b)):
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                nt.links.new(v, n.inputs[i])
        return n.outputs[0]

    glow = add(add(add(lobe((-1, 1.1, 1.0), 0.72, 0.97, 3.2), lobe((1, -0.6, 0.7), 0.55, 1.0, 0.45)),
                   lobe((0, 1, 0.25), 0.2, 1.0, 0.35)), 0.05)
    nt.links.new(glow, bg.inputs["Strength"])
    bg.inputs["Color"].default_value = (1, 1, 1, 1)


# ------------------------------------------------------------------ materials
def principled(name, color, metallic=0.0, rough=0.5, coat=0.0, coat_rough=0.08, emission=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Coat Roughness"].default_value = coat_rough
    if emission:
        b.inputs["Emission Color"].default_value = (*emission, 1)
        b.inputs["Emission Strength"].default_value = strength
    return m


class Nodes:
    """Short hands for wiring a material."""

    def __init__(self, material):
        self.nt = material.node_tree
        self.bsdf = self.nt.nodes["Principled BSDF"]
        self.coord = self.nt.nodes.new("ShaderNodeTexCoord")

    def new(self, kind, **props):
        n = self.nt.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def noise(self, scale, detail=2.0, stretch=None, coord="Object"):
        n = self.new("ShaderNodeTexNoise")
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        src = self.coord.outputs[coord]
        if stretch:
            mp = self.new("ShaderNodeMapping")
            mp.inputs["Scale"].default_value = stretch
            self.link(src, mp.inputs["Vector"])
            src = mp.outputs["Vector"]
        self.link(src, n.inputs["Vector"])
        return n.outputs["Fac"]

    def voronoi(self, scale, feature="F1", output="Distance"):
        n = self.new("ShaderNodeTexVoronoi", feature=feature)
        n.inputs["Scale"].default_value = scale
        self.link(self.coord.outputs["Object"], n.inputs["Vector"])
        return n.outputs[output]

    def math(self, op, a, b=None, c=None):
        n = self.new("ShaderNodeMath", operation=op)
        for i, v in enumerate((a, b, c)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                self.link(v, n.inputs[i])
        return n.outputs[0]

    def mix(self, fac, a, b, blend="MIX"):
        n = self.new("ShaderNodeMixRGB", blend_type=blend)
        for i, v in enumerate((fac, a, b)):
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            elif isinstance(v, tuple):
                n.inputs[i].default_value = (*v, 1) if len(v) == 3 else v
            else:
                self.link(v, n.inputs[i])
        return n.outputs[0]

    def ramp(self, fac, stops):
        n = self.new("ShaderNodeValToRGB")
        el = n.color_ramp.elements
        while len(el) < len(stops):
            el.new(0.5)
        for e, (pos, col) in zip(el, stops):
            e.position = pos
            e.color = (*col, 1) if len(col) == 3 else col
        self.link(fac, n.inputs["Fac"])
        return n.outputs["Color"]

    def bump(self, height, strength, distance=0.05, after=None):
        n = self.new("ShaderNodeBump")
        n.inputs["Strength"].default_value = strength
        n.inputs["Distance"].default_value = distance
        self.link(height, n.inputs["Height"])
        if after is not None:
            self.link(after, n.inputs["Normal"])
        return n.outputs["Normal"]

    def image(self, path, coord="Window", colorspace="sRGB"):
        n = self.new("ShaderNodeTexImage", extension="CLIP")
        n.image = bpy.data.images.load(path)
        n.image.colorspace_settings.name = colorspace
        self.link(self.coord.outputs[coord], n.inputs["Vector"])
        return n


def printed(material, print_path, raise_ink=True):
    """Lay the app's print over a finished material, as the camera sees it: ink with a little body, no shine."""
    n = Nodes(material)
    tex = n.image(print_path)
    b = n.bsdf
    def take(name):
        s = b.inputs[name]
        return s.links[0].from_socket if s.links else s.default_value
    base = take("Base Color")
    col = n.mix(tex.outputs["Alpha"], base if not hasattr(base, "__len__") else tuple(base), tex.outputs["Color"])
    n.link(col, b.inputs["Base Color"])
    for name, ink_value in (("Metallic", 0.0), ("Roughness", 0.55)):
        cur = take(name)
        inv = n.math("SUBTRACT", 1.0, tex.outputs["Alpha"])
        v = n.math("ADD", n.math("MULTIPLY", inv, cur), n.math("MULTIPLY", tex.outputs["Alpha"], ink_value))
        n.link(v, b.inputs[name])
    if raise_ink:
        prev = b.inputs["Normal"].links[0].from_socket if b.inputs["Normal"].links else None
        n.link(n.bump(tex.outputs["Alpha"], 0.25, 0.004, after=prev), b.inputs["Normal"])
    return material


def finish_material(finish, body_hex, seed=0.0):
    """The six paints and metals a pedal comes in. 0 enamel, 1 hammertone, 2 sparkle, 3 brushed, 4 crinkle, 5 anodised."""
    c = rgb(body_hex)
    deep = tuple(v * 0.62 for v in c)       # paint reads lighter under a bright key light than its swatch
    top = max(deep)
    if top > 0.36:                          # near-white paint clips to flat white under the key: hold it down
        deep = tuple(v * 0.36 / top for v in deep)
    if finish == 0:      # gloss enamel under a clear coat, with a little orange peel
        m = principled("enamel", deep, metallic=0.1, rough=0.5, coat=0.45, coat_rough=0.1)
        n = Nodes(m)
        n.link(n.bump(n.noise(38.0), 0.022), n.bsdf.inputs["Normal"])
    elif finish == 1:    # hammertone: metallic paint pulled into dimples
        m = principled("hammertone", deep, metallic=0.75, rough=0.34, coat=0.3, coat_rough=0.15)
        n = Nodes(m)
        cells = n.voronoi(9.0, "SMOOTH_F1")
        n.link(n.bump(cells, 0.5, 0.03), n.bsdf.inputs["Normal"])
        n.link(n.mix(n.math("MULTIPLY", cells, 0.9), tuple(v * 1.25 for v in deep), tuple(v * 0.6 for v in deep)), n.bsdf.inputs["Base Color"])
    elif finish == 2:    # sparkle: metal flake under a deep clear coat
        m = principled("sparkle", deep, metallic=0.55, rough=0.36, coat=1.0, coat_rough=0.04)
        n = Nodes(m)
        flake = n.voronoi(520.0, "F1", "Color")
        sep = n.new("ShaderNodeSeparateColor")
        n.link(flake, sep.inputs["Color"])
        hot = n.math("GREATER_THAN", sep.outputs[0], 0.93)
        n.link(n.mix(hot, deep, (0.95, 0.95, 0.9)), n.bsdf.inputs["Base Color"])
        n.link(n.math("MULTIPLY_ADD", hot, -0.2, 0.36), n.bsdf.inputs["Roughness"])
        nm = n.new("ShaderNodeBump")
        nm.inputs["Strength"].default_value = 0.35
        nm.inputs["Distance"].default_value = 0.01
        n.link(sep.outputs[1], nm.inputs["Height"])
        n.link(nm.outputs["Normal"], n.bsdf.inputs["Normal"])
    elif finish == 3:    # bare brushed aluminium: the grain runs across the box
        m = principled("brushed", tuple(min(1.0, v * 1.15) for v in c), metallic=1.0, rough=0.34)
        n = Nodes(m)
        grain = n.noise(60.0, 3.0, stretch=(0.02, 14.0, 14.0))
        n.link(n.bump(grain, 0.12, 0.01), n.bsdf.inputs["Normal"])
        n.link(n.math("MULTIPLY_ADD", grain, 0.16, 0.26), n.bsdf.inputs["Roughness"])
    elif finish == 4:    # crinkle: wrinkle paint, matt, fine and tight
        base = tuple(max(v, 0.012) for v in deep)
        m = principled("crinkle", base, metallic=0.0, rough=0.62)
        n = Nodes(m)
        w = n.voronoi(34.0, "SMOOTH_F1")
        w2 = n.noise(120.0, 3.0)
        n.link(n.bump(n.math("ADD", w, n.math("MULTIPLY", w2, 0.4)), 0.55, 0.02), n.bsdf.inputs["Normal"])
    else:                # anodised: dyed satin metal
        dye = tuple(max(v * 0.9, 0.01) for v in c)
        top = max(dye)
        if top > 0.5:                       # a pale dye on bright metal burns out to white: hold it down
            dye = tuple(v * 0.5 / top for v in dye)
        m = principled("anodised", dye, metallic=0.85, rough=0.42)
        n = Nodes(m)
        n.link(n.bump(n.noise(260.0, 2.0), 0.04, 0.005), n.bsdf.inputs["Normal"])
    return m


def chrome():   return principled("chrome", (0.92, 0.92, 0.93), metallic=1.0, rough=0.08)
def nickel():   return principled("nickel", (0.78, 0.76, 0.72), metallic=1.0, rough=0.28)
def steel():    return principled("steel", (0.55, 0.55, 0.56), metallic=1.0, rough=0.38)
def black():    return principled("black", (0.01, 0.01, 0.01), rough=0.6)
def rubber():   return principled("rubber", (0.012, 0.012, 0.012), rough=0.78)
def bakelite(): return principled("bakelite", (0.006, 0.006, 0.007), rough=0.22)


def wood(seed=0.0):
    """Varnished hardwood, the grain running the length of the box."""
    m = principled("wood", (0.2, 0.09, 0.04), rough=0.45, coat=0.4, coat_rough=0.16)
    n = Nodes(m)
    mp = n.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (6.0, 0.22, 6.0)
    mp.inputs["Location"].default_value = (seed, seed * 0.37, 0.0)
    n.link(n.coord.outputs["Object"], mp.inputs["Vector"])
    nz = n.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 1.6
    nz.inputs["Detail"].default_value = 6.0
    nz.inputs["Distortion"].default_value = 1.4
    n.link(mp.outputs["Vector"], nz.inputs["Vector"])
    wave = n.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction="X")
    wave.inputs["Scale"].default_value = 2.2
    wave.inputs["Distortion"].default_value = 5.0
    wave.inputs["Detail"].default_value = 3.0
    n.link(mp.outputs["Vector"], wave.inputs["Vector"])
    f = n.math("ADD", n.math("MULTIPLY", wave.outputs["Fac"], 0.6), n.math("MULTIPLY", nz.outputs["Fac"], 0.5))
    n.link(n.ramp(f, [(0.32, (0.008, 0.003, 0.0015)), (0.55, (0.04, 0.014, 0.005)), (0.78, (0.115, 0.044, 0.014))]), n.bsdf.inputs["Base Color"])
    n.link(n.bump(f, 0.05, 0.01), n.bsdf.inputs["Normal"])
    return m


# ------------------------------------------------------------------ geometry
def link(me, name, material, smooth=True):
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if material is not None:
        me.materials.append(material)
    for p in me.polygons:
        p.use_smooth = smooth
    return ob


def lathe(name, profile, material, seg=96, flutes=0, depth=0.0, flute_rings=(0, -1), at=(0, 0, 0), power=1.6):
    """Turn a profile of (radius, height) points round the z axis. Flutes are cut into the named rings."""
    bm = bmesh.new()
    rings = []
    lo, hi = flute_rings
    if hi < 0:
        hi = len(profile) + hi
    for i, (r, z) in enumerate(profile):
        ring = []
        for s in range(seg):
            a = 2 * math.pi * s / seg
            rr = r
            if flutes and lo <= i <= hi:
                rr = r * (1 - depth * (0.5 - 0.5 * math.cos(a * flutes)) ** power)
            ring.append(bm.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        rings.append(ring)
    for a, b in zip(rings, rings[1:]):
        for s in range(seg):
            bm.faces.new((a[s], a[(s + 1) % seg], b[(s + 1) % seg], b[s]))
    bm.faces.new(rings[-1])
    bm.faces.new(list(reversed(rings[0])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(me, name, material)
    ob.location = at
    return ob


def roll(ob, width, segments=6, angle=40):
    """Roll every edge sharper than `angle` degrees: what a casting, a moulding or a file leaves."""
    return bevel(ob, width, segments, angle)


def bevel(ob, width, segments=4, angle=30):
    mod = ob.modifiers.new("bevel", "BEVEL")
    mod.width, mod.segments = width, segments
    mod.limit_method, mod.angle_limit = "ANGLE", math.radians(angle)
    ob.modifiers.new("normals", "WEIGHTED_NORMAL")
    return ob


def prism(name, sides, radius, z0, z1, material, at=(0, 0, 0), rot=0.0, round_edges=0.0, smooth=False):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=sides, radius1=radius, radius2=radius, depth=z1 - z0)
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0, 0, (z0 + z1) / 2))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(me, name, material, smooth)
    ob.location = at
    ob.rotation_euler[2] = rot
    if round_edges:
        bevel(ob, round_edges, 3, 20)
    return ob


def box(name, x0, x1, y0, y1, z0, z1, material, corner=0.0, edge=0.0, edge_segments=6, taper=(1.0, 1.0), smooth=True, corner_segments=10):
    """A block between two corners. `corner` rounds the upright edges, `edge` rolls every other one,
    `taper` makes the top smaller than the bottom the way a casting leaves its mould."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, verts=bm.verts, vec=(x1 - x0, y1 - y0, z1 - z0))
    for v in bm.verts:
        if v.co.z > 0:
            v.co.x *= taper[0]
            v.co.y *= taper[1]
    if corner > 0:
        upright = [e for e in bm.edges if abs(e.verts[0].co.z - e.verts[1].co.z) > (z1 - z0) * 0.5]
        bmesh.ops.bevel(bm, geom=upright, offset=corner, segments=corner_segments, affect="EDGES", profile=0.5)
    bmesh.ops.translate(bm, verts=bm.verts, vec=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(me, name, material, smooth)
    if edge > 0:
        mod = ob.modifiers.new("edge", "BEVEL")
        mod.width, mod.segments = edge, edge_segments
        mod.limit_method, mod.angle_limit = "ANGLE", math.radians(40)
        ob.modifiers.new("normals", "WEIGHTED_NORMAL")
    return ob


def extrude_profile(name, profile_yz, x0, x1, material, edge=0.0, edge_segments=3):
    """A side-view outline of (y, z) points pushed across from x0 to x1."""
    bm = bmesh.new()
    left = [bm.verts.new((x0, y, z)) for y, z in profile_yz]
    right = [bm.verts.new((x1, y, z)) for y, z in profile_yz]
    n = len(profile_yz)
    for i in range(n):
        bm.faces.new((left[i], left[(i + 1) % n], right[(i + 1) % n], right[i]))
    bm.faces.new(left)
    bm.faces.new(list(reversed(right)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(me, name, material, smooth=True)
    if edge > 0:
        mod = ob.modifiers.new("edge", "BEVEL")
        mod.width, mod.segments = edge, edge_segments
        mod.limit_method, mod.angle_limit = "ANGLE", math.radians(12)
        ob.modifiers.new("normals", "WEIGHTED_NORMAL")
    return ob


def cut(body, cutter, material_from_cutter=True):
    """Take `cutter` out of `body`. The cutter is hidden afterwards. Cut first, roll the edges after."""
    mod = body.modifiers.new("cut", "BOOLEAN")
    mod.operation, mod.object, mod.solver = "DIFFERENCE", cutter, "EXACT"
    if material_from_cutter and hasattr(mod, "material_mode"):
        mod.material_mode = "TRANSFER"
    mod.name = "cut-" + cutter.name
    cutter.hide_render = True
    cutter.hide_viewport = True
    cutter.display_type = "WIRE"
    return body


def grid(ob, step):
    """Cut a mesh into a grid of cells about `step` across, flat faces and all.

    A hole cut through one big face leaves long thin triangles running from the hole to the far
    corners, and the renderer shows the edge between two of them as a hairline straight across the
    top of the box. Cut into small cells first, the hole only touches the cells round it."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    lo = Vector([min(v.co[k] for v in bm.verts) for k in range(3)])
    hi = Vector([max(v.co[k] for v in bm.verts) for k in range(3)])
    for k in (0, 1):
        n = max(1, int((hi[k] - lo[k]) / step))
        for i in range(1, n):
            co = Vector((0, 0, 0)); co[k] = lo[k] + (hi[k] - lo[k]) * i / n
            no = Vector((0, 0, 0)); no[k] = 1.0
            bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=co, plane_no=no)
    bm.to_mesh(ob.data)
    bm.free()
    return ob


def stand(ob, point, normal, turn=0.0):
    """Stand a part built along +z on a surface at `point`, facing along `normal`."""
    q = Vector((0, 0, 1)).rotation_difference(normal)
    ob.rotation_mode = "QUATERNION"
    from mathutils import Quaternion
    ob.rotation_quaternion = q @ Quaternion((0, 0, 1), turn)
    ob.location = point
    return ob
