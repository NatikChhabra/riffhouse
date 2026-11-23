# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Build and photograph every amp.

    blender -b -P amps.py -- <gear folder written by `Riffhouse --snapshots`> <output folder> [--only id,id] [--samples n] [--scale s]

The amp page is seen from the front, so (as for the cab's mic) the shared stage's "floor" is the
wall behind the amp and the amp lies on it face up: height is distance out toward the player. The
app says where every part sits (gear.json) and hands over the print (panel lettering and the flat
parts of the logo) and the raised parts of the logo as outlines. This script builds the cabinet
round them: the covering, the corners, the handle, the cloth or the vents, the panel. The parts the
app photographs on their own and lays on top stay out: knobs, the input socket, the panel screws,
the pilot lamp and its switch.

Needs Pillow, NumPy and Shapely in Blender's own Python (blender/4.x/python/bin/python3 -m pip install pillow shapely).
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402
from common import CM  # noqa: E402
from mathutils import Vector  # noqa: E402

D = 2.4            # the front of the cabinet, out from the wall, cm
PANEL_SINK = 0.32  # how far the control panel sits back under the cabinet's edge
CLOTH_SINK = 0.42  # and the grille cloth


def slug(name):
    return "".join(ch if ch.isalnum() else "_" for ch in name.lower())


# ------------------------------------------------------------------ the app's outlines
def parse_path(text):
    """A JUCE Path string as a list of closed polygons (curves flattened)."""
    tok = text.replace(",", " ").split()
    polys, cur, i, cmd = [], [], 0, None
    pos = (0.0, 0.0)
    arity = {"m": 2, "l": 2, "q": 4, "c": 6, "z": 0}
    while i < len(tok):
        t = tok[i]
        if t in ("a", "n"):          # winding flag
            i += 1
            continue
        if t in arity:
            cmd = t
            i += 1
            if cmd == "z":
                if len(cur) > 2:
                    polys.append(cur)
                cur = []
                continue
        n = arity[cmd]
        v = [float(x) for x in tok[i:i + n]]
        i += n
        if cmd == "m":
            if len(cur) > 2:
                polys.append(cur)
            cur = [(v[0], v[1])]
            pos = (v[0], v[1])
            cmd = "l"
        elif cmd == "l":
            cur.append((v[0], v[1]))
            pos = (v[0], v[1])
        elif cmd == "q":
            p0 = pos
            for s in range(1, 7):
                u = s / 6
                cur.append(((1 - u) ** 2 * p0[0] + 2 * (1 - u) * u * v[0] + u * u * v[2],
                            (1 - u) ** 2 * p0[1] + 2 * (1 - u) * u * v[1] + u * u * v[3]))
            pos = (v[2], v[3])
        elif cmd == "c":
            p0 = pos
            for s in range(1, 9):
                u = s / 8
                a, b, cc, d = (1 - u) ** 3, 3 * (1 - u) ** 2 * u, 3 * (1 - u) * u * u, u ** 3
                cur.append((a * p0[0] + b * v[0] + cc * v[2] + d * v[4], a * p0[1] + b * v[1] + cc * v[3] + d * v[5]))
            pos = (v[4], v[5])
    if len(cur) > 2:
        polys.append(cur)
    # drop the closing point that repeats the first
    out = []
    for p in polys:
        if len(p) > 3 and abs(p[0][0] - p[-1][0]) < 1e-4 and abs(p[0][1] - p[-1][1]) < 1e-4:
            p = p[:-1]
        out.append(p)
    return out


def inside(pt, poly):
    x, y = pt
    n, hit = len(poly), False
    for i in range(n):
        x0, y0 = poly[i]; x1, y1 = poly[(i + 1) % n]
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            hit = not hit
    return hit


def islands(polys):
    """Outlines grouped as [outer, its holes...]: an outline inside an odd number of others is a hole."""
    clean = []
    for p in polys:
        q = [p[0]]
        for pt in p[1:]:
            if abs(pt[0] - q[-1][0]) > 1e-3 or abs(pt[1] - q[-1][1]) > 1e-3:
                q.append(pt)
        if len(q) > 2:
            clean.append(q)
    # fonts and JUCE wind solid outlines one way and holes the other, and solid parts may overlap
    def area(p):
        return 0.5 * sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))
    areas = [area(p) for p in clean]
    if not clean:
        return []
    solid_sign = 1.0 if areas[max(range(len(clean)), key=lambda i: abs(areas[i]))] > 0 else -1.0
    groups = {i: [clean[i]] for i in range(len(clean)) if areas[i] * solid_sign > 0}
    for i, p in enumerate(clean):
        if areas[i] * solid_sign < 0:
            owners = sorted((j for j in groups if inside(p[0], clean[j])), key=lambda j: abs(areas[j]))
            if owners:
                groups[owners[0]].append(p)
    return list(groups.values())


def raised_shape(st, name, polys, z_base, height, material, bevel):
    """An outline (holes and all) pushed up off a face by `height`, its edges rounded: one piece per island.
    The outlines are cleaned first (shapely), so a self-crossing glyph or overlapping strokes come out solid."""
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    shapes = []
    for group in islands(polys):
        try:
            shapes.append(Polygon(group[0], group[1:]).buffer(0))
        except Exception:
            continue
    merged = unary_union(shapes) if shapes else None
    if merged is None or merged.is_empty:
        return
    parts = list(merged.geoms) if hasattr(merged, "geoms") else [merged]
    for k, poly in enumerate(parts):
        rings = [list(poly.exterior.coords)[:-1]] + [list(r.coords)[:-1] for r in poly.interiors]
        one(st, "%s-%d" % (name, k), rings, z_base, height, material, bevel)


def one(st, name, polys, z_base, height, material, bevel):
    """One island (an outline and its holes) as a solid: capped top and bottom, walls all round."""
    import bmesh
    from mathutils.geometry import tessellate_polygon
    loops = [[st.at(x, y, z_base) for (x, y) in poly] for poly in polys]
    flat = [v for loop in loops for v in loop]
    tris = tessellate_polygon([[(v.x, v.y, 0.0) for v in loop] for loop in loops])
    bm = bmesh.new()
    lo = [bm.verts.new((v.x, v.y, z_base)) for v in flat]
    hi = [bm.verts.new((v.x, v.y, z_base + height)) for v in flat]
    for t in tris:
        try:
            bm.faces.new((hi[t[0]], hi[t[1]], hi[t[2]]))
            bm.faces.new((lo[t[2]], lo[t[1]], lo[t[0]]))
        except ValueError:
            pass
    start = 0
    for loop in loops:
        n = len(loop)
        for i in range(n):
            a0, a1 = start + i, start + (i + 1) % n
            try:
                bm.faces.new((lo[a0], lo[a1], hi[a1], hi[a0]))
            except ValueError:
                pass
        start += n
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # a self-crossing outline can turn the whole cap over: the top always faces out
    for f in bm.faces:
        zs = [v.co.z for v in f.verts]
        f.normal_update()
        if min(zs) > z_base + height * 0.5 and f.normal.z < 0:
            f.normal_flip()
        elif max(zs) < z_base + height * 0.5 and f.normal.z > 0:
            f.normal_flip()
    me = c.bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = c.link(me, name, material, smooth=False)
    mod = ob.modifiers.new("edge", "BEVEL")
    mod.width, mod.segments = bevel, 3
    mod.limit_method, mod.angle_limit = "ANGLE", math.radians(50)
    mod.harden_normals = True
    return ob


# ------------------------------------------------------------------ materials
def tolex(kind, colour, seed):
    col = c.rgb(colour)
    if kind == 1:   # lacquered tweed: a diagonal twill under amber lacquer
        m = c.principled("tweed", (0.32, 0.2, 0.08), rough=0.42, coat=0.6, coat_rough=0.22)
        n = c.Nodes(m)
        mp = n.new("ShaderNodeMapping")
        mp.inputs["Rotation"].default_value = (0, 0, math.radians(-38))
        n.link(n.coord.outputs["Object"], mp.inputs["Vector"])
        wave = n.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction="X")
        wave.inputs["Scale"].default_value = 26.0
        wave.inputs["Distortion"].default_value = 0.6
        wave.inputs["Detail"].default_value = 2.0
        n.link(mp.outputs["Vector"], wave.inputs["Vector"])
        fine = n.noise(160.0, 4.0)
        f = n.math("ADD", n.math("MULTIPLY", wave.outputs["Fac"], 0.7), n.math("MULTIPLY", fine, 0.5))
        n.link(n.ramp(f, [(0.3, (0.07, 0.035, 0.012)), (0.6, (0.3, 0.19, 0.08)), (0.85, (0.46, 0.32, 0.15))]), n.bsdf.inputs["Base Color"])
        n.link(n.bump(f, 0.6, 0.02), n.bsdf.inputs["Normal"])
        return m
    if kind == 3:   # cream levant: pale, so held well under the key light
        base = tuple(v * 0.5 for v in col)
    else:
        base = tuple(max(0.0025, v * 0.3) for v in col)
    m = c.principled("levant", base, rough=0.72)
    n = c.Nodes(m)
    cells = n.voronoi(24.0 + seed, "SMOOTH_F1")
    fine = n.noise(110.0, 3.0)
    h = n.math("ADD", n.math("MULTIPLY", cells, 1.0), n.math("MULTIPLY", fine, 0.25))
    n.link(n.bump(h, 0.5, 0.015), n.bsdf.inputs["Normal"])
    # the tops of the pebbles wear a touch shinier than the valleys
    n.link(n.math("MULTIPLY_ADD", cells, 0.25, 0.5), n.bsdf.inputs["Roughness"])
    return m


def panel_material(kind, seed=0):
    """0 black, 1 chrome, 2 gold, 3 plate, 4 anodised, 5 brushed."""
    if kind == 0:
        m = c.principled("panel-black", (0.004, 0.004, 0.0045), rough=0.42)
        n = c.Nodes(m)
        n.link(n.bump(n.noise(90.0, 2.0), 0.03, 0.005), n.bsdf.inputs["Normal"])
        return m
    if kind == 1:
        m = c.principled("panel-chrome", (0.86, 0.87, 0.88), metallic=1.0, rough=0.2)
        n = c.Nodes(m)
        grain = n.noise(60.0, 3.0, stretch=(0.02, 14.0, 14.0))
        n.link(n.bump(grain, 0.08, 0.01), n.bsdf.inputs["Normal"])
        n.link(n.math("MULTIPLY_ADD", grain, 0.12, 0.14), n.bsdf.inputs["Roughness"])
        return m
    if kind in (2, 5):
        col = (0.62, 0.43, 0.15) if kind == 2 else (0.42, 0.43, 0.44)
        m = c.principled("panel-brushed", col, metallic=1.0, rough=0.34)
        n = c.Nodes(m)
        grain = n.noise(90.0, 4.0, stretch=(0.015, 18.0, 18.0))
        n.link(n.bump(grain, 0.55, 0.01), n.bsdf.inputs["Normal"])
        n.link(n.math("MULTIPLY_ADD", grain, 0.2, 0.26), n.bsdf.inputs["Roughness"])
        return m
    if kind == 3:
        m = c.principled("panel-plate", (0.6, 0.61, 0.62), metallic=1.0, rough=0.32)
        return m
    m = c.principled("panel-anodised", (0.03, 0.032, 0.035), metallic=0.85, rough=0.55)
    n = c.Nodes(m)
    n.link(n.bump(n.noise(260.0, 2.0), 0.04, 0.005), n.bsdf.inputs["Normal"])
    return m


def with_print(m, print_path):
    return c.printed(m, print_path, raise_ink=True)


# ------------------------------------------------------------------ grille cloth, drawn thread by thread
def cloth_maps(st, rect, weave, piping_round, speakers, work, aid):
    """Colour and height pictures of the cloth, the size of the whole render (laid on through the camera)."""
    W, H = st.scene.render.resolution_x, st.scene.render.resolution_y
    ax, ay, aw, ah = st.area
    s = W / aw
    x0, y0 = (rect[0] - ax) * s, (rect[1] - ay) * s
    x1, y1 = x0 + rect[2] * s, y0 + rect[3] * s
    rnd = np.random.default_rng(97 + weave)
    col = np.zeros((H, W, 3), np.float32)
    hgt = np.zeros((H, W), np.float32)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    u, v = (xx - x0) / s, (yy - y0) / s          # nominal pixels across the cloth
    if weave in (0, 1, 4):     # plain weave: warp and weft over and under, every 2.5 px
        p = 2.5
        base = {0: (0.02, 0.022, 0.025), 1: (0.16, 0.1, 0.04), 4: (0.01, 0.01, 0.011)}[weave]
        thread = {0: (0.2, 0.21, 0.225), 1: (0.38, 0.27, 0.13), 4: (0.045, 0.046, 0.05)}[weave]
        fu, fv = (u / p) % 1.0, (v / p) % 1.0
        cu, cv = np.floor(u / p), np.floor(v / p)
        over = ((cu + cv) % 2) == 0
        warp = np.sin(np.pi * fu) ** 0.7
        weft = np.sin(np.pi * fv) ** 0.7
        h = np.where(over, warp * (0.6 + 0.4 * weft), weft * (0.6 + 0.4 * warp))
        jitter = rnd.uniform(0.8, 1.15, size=(int(H / (p * s)) + 3, int(W / (p * s)) + 3)).astype(np.float32)
        jj = jitter[np.clip(cv.astype(int), 0, jitter.shape[0] - 1), np.clip(cu.astype(int), 0, jitter.shape[1] - 1)]
        t = np.clip(h * jj, 0, 1)[..., None]
        col = np.array(base) * (1 - t) + np.array(thread) * t
        hgt = h
        if weave == 0:           # every fourth weft thread is the silver one, with a faint blue cast
            silver = ((cv % 4) == 0) & ~over
            col = np.where(silver[..., None], col * np.array((1.25, 1.32, 1.45)), col)
            for _ in range(0):
                tx = rnd.uniform(0, rect[2]); ty = p * math.floor(rnd.uniform(0, rect[3]) / p)
                ln = rnd.uniform(2, 6)
                tint = (0.45, 0.6, 0.78) if rnd.integers(5) == 0 else (0.75, 0.76, 0.78)
                a = rnd.uniform(0.15, 0.5)
                X0, X1 = int(x0 + tx * s), int(x0 + (tx + ln) * s)
                Y0, Y1 = int(y0 + ty * s), int(y0 + (ty + 1.0) * s)
                col[Y0:Y1, X0:X1] = col[Y0:Y1, X0:X1] * (1 - a) + np.array(tint) * a
        if weave == 1:           # darker stripes woven in a hand apart
            band = ((u - 9.0) % 18.0) < 2.4
            col = np.where(band[..., None], col * 0.72, col)
    elif weave == 2:           # windowpane cloth: a brass lattice of upright rectangles over deep green
        pu, pv = 30.0, 20.0
        du = ((u % pu) / pu - 0.5) * pu
        dv = ((v % pv) / pv - 0.5) * pv
        line = np.maximum(np.exp(-((np.abs(du) - pu * 0.5 + 0.7) / 1.0) ** 2),
                          np.exp(-((np.abs(dv) - pv * 0.5 + 0.7) / 1.0) ** 2))
        fine = (np.sin(u * math.pi / 1.2) * np.sin(v * math.pi / 1.2)) * 0.5 + 0.5
        col = np.array((0.012, 0.04, 0.03)) * (1 - line[..., None]) + np.array((0.4, 0.34, 0.17)) * line[..., None]
        col = col * (0.85 + 0.15 * fine[..., None])
        hgt = line * 0.8 + fine * 0.3
    elif weave == 3:           # salt and pepper basket weave
        p = 3.4
        cu, cv = np.floor(u / p), np.floor(v / p)
        fu, fv = (u / p) % 1.0, (v / p) % 1.0
        across = ((cu + cv) % 2) == 0
        pair_u = np.maximum(np.exp(-((fu - 0.28) / 0.13) ** 2), np.exp(-((fu - 0.72) / 0.13) ** 2))
        pair_v = np.maximum(np.exp(-((fv - 0.28) / 0.13) ** 2), np.exp(-((fv - 0.72) / 0.13) ** 2))
        h = np.where(across, pair_v, pair_u)
        light = np.where(across, 1.0, 0.0)
        c_light, c_dark = np.array((0.52, 0.5, 0.45)), np.array((0.12, 0.115, 0.1))
        tone = (c_light * light[..., None] + c_dark * (1 - light[..., None]))
        col = np.array((0.01, 0.01, 0.01)) * (1 - h[..., None]) + tone * h[..., None]
        hgt = h
    elif weave == 5:           # perforated steel: round holes on a hex pitch
        p = 9.0
        row = np.floor((v - 4.0) / (p * 0.866))
        cx = np.round((u - 4.0 - (row % 2) * p * 0.5) / p) * p + 4.0 + (row % 2) * p * 0.5
        cy = row * p * 0.866 + 4.0
        r = np.hypot(u - cx, v - cy)
        hole = np.clip((3.2 - r) / 0.5, 0, 1)
        col = np.array((0.07, 0.072, 0.075)) * (1 - hole[..., None]) + np.array((0.004, 0.004, 0.004)) * hole[..., None]
        hgt = 1.0 - hole
    elif weave == 6:           # a woven metal grid
        p = 4.0
        g1 = np.exp(-(((u % p) - p / 2) / 0.6) ** 2)
        g2 = np.exp(-(((v % p) - p / 2) / 0.6) ** 2)
        g = np.maximum(g1, g2)
        col = np.array((0.012, 0.013, 0.015)) * (1 - g[..., None]) + np.array((0.16, 0.18, 0.2)) * g[..., None]
        hgt = g
    # the speaker shows faintly through the cloth: a darker round, its cone and the dust cap
    for (sx, sy, sr) in speakers:
        cx, cy, R = (sx - ax) * s, (sy - ay) * s, sr * s
        rr = np.hypot(xx - cx, yy - cy) / R
        shade = np.clip(1.0 - rr, 0, 1) ** 0.35 * (rr < 1.0)
        ring = np.exp(-((rr - 0.96) / 0.05) ** 2) * 0.18
        cap = np.clip(1.0 - rr / 0.28, 0, 1) ** 0.5 * 0.25
        dark = np.clip(0.42 * shade + ring - cap, 0, 0.6)
        col = col * (1.0 - dark[..., None])
    inside = (xx >= x0) & (xx < x1) & (yy >= y0) & (yy < y1)
    col = np.where(inside[..., None], col, 0.0)
    hgt = np.where(inside, hgt, 0.0)
    cp, hp = os.path.join(work, aid + "-cloth.png"), os.path.join(work, aid + "-cloth-h.png")
    # stored as linear light: loaded back as Non-Color
    Image.fromarray((np.clip(col, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(cp)
    Image.fromarray((np.clip(hgt, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(hp)
    return cp, hp


def cloth_material(colour_path, height_path, metal):
    m = c.principled("cloth", (0.05, 0.05, 0.05), metallic=0.9 if metal else 0.0, rough=0.6 if metal else 0.82)
    n = c.Nodes(m)
    col = n.image(colour_path, colorspace="Non-Color")
    h = n.image(height_path, colorspace="Non-Color")
    n.link(col.outputs["Color"], n.bsdf.inputs["Base Color"])
    n.link(n.bump(h.outputs["Color"], 0.55, 0.015), n.bsdf.inputs["Normal"])
    if not metal:
        n.bsdf.inputs["Sheen Weight"].default_value = 0.4
    return m


# ------------------------------------------------------------------ hardware
def piping_loop(st, rect, rad, z, colour, thick=0.075):
    """A cord of piping round a rounded rectangle, lying on a face at height z."""
    gx, gy, gw, gh = rect
    pip = c.principled("piping", tuple(v * 0.8 for v in c.rgb(colour)), metallic=0.3, rough=0.35, coat=0.4)
    cu = c.bpy.data.curves.new("piping", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = thick
    cu.bevel_resolution = 4
    sp = cu.splines.new("POLY")
    pts = []
    for k in range(4):
        cxr = gx + gw - rad if k in (0, 1) else gx + rad
        cyr = gy + gh - rad if k in (1, 2) else gy + rad
        for s in range(9):
            ang = math.radians(-90 + 90 * k + 90 * s / 8)
            pts.append((cxr + rad * math.cos(ang), cyr + rad * math.sin(ang)))
    sp.points.add(len(pts) - 1)
    for k, (x, y) in enumerate(pts):
        w = st.at(x, y, z + thick * 0.6)
        sp.points[k].co = (w.x, w.y, w.z, 1.0)
    sp.use_cyclic_u = True
    ob = c.bpy.data.objects.new("piping", cu)
    c.bpy.context.scene.collection.objects.link(ob)
    cu.materials.append(pip)
    return ob


def corner_caps(st, rect, rad, z0, z1, size, metal):
    """Steel corner protectors: the cabinet's own rounded corner, a little bigger, cut to a square."""
    x, y, w, h = rect
    e = 0.12
    a, b = st.at(x, y, z1), st.at(x + w, y + h, z1)
    shell = c.box("capshell", a.x - e, b.x + e, b.y - e, a.y + e, z0, z1 + e, metal, corner=rad + e, corner_segments=10)
    c.grid(shell, 1.0)
    for i in range(4):
        right, bottom = i % 2 == 1, i >= 2
        cx0 = x + w - size if right else x - 4
        cx1 = x + w + 4 if right else x + size
        cy0 = y + h - size if bottom else y - 4
        cy1 = y + h + 4 if bottom else y + size
        p, q = st.at(cx0, cy0, z1), st.at(cx1, cy1, z1)
        piece = c.box("cap%d" % i, p.x, q.x, q.y, p.y, z0 - 1, z1 + 1, metal)
        piece.data = shell.data.copy() if False else piece.data
        mod = piece.modifiers.new("keep", "BOOLEAN")
        mod.operation, mod.object, mod.solver = "INTERSECT", shell, "EXACT"
        # the cap's own edges, rounded by the stamping
        c.roll(piece, 0.3, 6, 40)
        # a domed rivet in the middle of each face
        rx = x + w - size * 0.42 if right else x + size * 0.42
        ry = y + h - size * 0.42 if bottom else y + size * 0.42
        rv = c.lathe("rivet", [(0.0001, 0.0), (0.22, 0.0), (0.2, 0.06), (0.12, 0.11), (0.0001, 0.125)], metal, seg=32)
        rv.location = st.at(rx, ry, z1 + e)
    shell.hide_render = True


def strap_handle(st, rect, top_y, zc, leather, metal):
    """A leather strap riding up off the top in an arc, held at each end by a chromed cap."""
    x, y, w, h = rect
    obs = []
    n = 24
    half = w * 0.5 - 22
    thick = 0.42
    width = 2.2
    rise = max(4.0, (top_y - y) - 1.0)
    import bmesh
    bm = bmesh.new()
    rows = []
    for i in range(n + 1):
        t = i / n
        xx = x + 22 + t * (w - 44)
        arc = math.sin(math.pi * t) ** 0.6
        yy = top_y - 2 - rise * arc
        p = st.at(xx, yy, zc)
        rows.append([bm.verts.new((p.x, p.y + dy, zc + dz)) for dy, dz in ((0, -width / 2), (thick, -width / 2), (thick, width / 2), (0, width / 2))])
    for a, b in zip(rows, rows[1:]):
        for k in range(4):
            bm.faces.new((a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]))
    bm.faces.new(rows[0]); bm.faces.new(list(reversed(rows[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = c.bpy.data.meshes.new("strap")
    bm.to_mesh(me); bm.free()
    strap = c.link(me, "strap", leather)
    c.roll(strap, 0.12, 3, 30)
    sub = strap.modifiers.new("smooth", "SUBSURF"); sub.levels = sub.render_levels = 1
    for side in (0, 1):
        cx = x + 14 if side == 0 else x + w - 14
        a, b = st.at(cx - 18, top_y - 1, zc), st.at(cx + 18, top_y + 1, zc)
        cap = c.box("endcap", a.x, b.x, b.y - 0.1, a.y + 0.75, zc - 1.4, zc + 1.4, metal, corner=0.25)
        c.roll(cap, 0.25, 6, 30)
        obs.append(cap)
    return obs


def bar_handle(st, rect, top_y, zc, metal):
    x, y, w, h = rect
    rise = (top_y - y)
    pa, pb = st.at(x + 16, top_y - rise * 0.8, zc), st.at(x + w - 16, top_y - rise * 0.8, zc)
    bar = c.prism("bar", 32, 0.32, 0, (pb - pa).length, metal, smooth=True)
    bar.rotation_euler = (0, math.pi / 2, 0)
    bar.location = pa
    for xx in (x + 16, x + w - 16):
        p0, p1 = st.at(xx, top_y, zc), st.at(xx, top_y - rise * 0.8, zc)
        post = c.prism("post", 32, 0.3, 0, (p1 - p0).length + 0.3, metal, smooth=True)
        post.rotation_euler = (-math.pi / 2, 0, 0)
        post.location = p0
        foot = c.box("foot", p0.x - 0.6, p0.x + 0.6, p0.y - 0.05, p0.y + 0.25, zc - 0.6, zc + 0.6, metal, corner=0.2)
        c.roll(foot, 0.12, 4)


def screw_head(st, x, y, z, r, metal, turn):
    sc = c.lathe("screw", [(0.0001, 0.0), (r, 0.0), (r, 0.05), (r * 0.8, r * 0.38), (0.0001, r * 0.46)], metal, seg=40)
    sc.location = st.at(x, y, z)
    slot = c.box("slot", -r * 0.85, r * 0.85, -r * 0.1, r * 0.1, r * 0.25, r * 0.6, c.principled("slot", (0.005, 0.005, 0.005), rough=0.7))
    slot.location = st.at(x, y, z)
    slot.rotation_euler[2] = turn


# ------------------------------------------------------------------ one amp
def build(a, gear_dir, out_dir, samples, scale):
    aid, form = a["id"], a["form"]
    work = os.path.join(out_dir, "_work")
    os.makedirs(work, exist_ok=True)
    st = c.Stage(a["area"], D, scale=scale, samples=samples, floor=True)
    pr = os.path.join(gear_dir, aid + "-print.png")
    hx, hy, hw, hh = a["head"]
    px, py, pw, ph = a["panel"]
    gx, gy, gw, gh = a["grille"]
    cabinet = form not in (3, 5)
    metal = c.principled("corner", (0.72, 0.72, 0.73), metallic=1.0, rough=0.2)
    nickel = c.nickel()
    leather = c.principled("leather", (0.018, 0.014, 0.012), rough=0.5, coat=0.15)
    cn = c.Nodes(leather)
    cn.link(cn.bump(cn.voronoi(40.0, "SMOOTH_F1"), 0.2, 0.01), cn.bsdf.inputs["Normal"])

    def rect_box(name, r, z0, z1, mat, corner=0.0):
        p, q = st.at(r[0], r[1], z1), st.at(r[0] + r[2], r[1] + r[3], z1)
        return c.box(name, p.x, q.x, q.y, p.y, z0, z1, mat, corner=corner)

    if form == 5:
        # the road case round the unit: covered ply, an aluminium lip round the bay, blank panels
        rk = a["rack"]
        case = rect_box("case", rk, 0.0, D, with_print(tolex(0, "141516", 1), pr), corner=10 * CM)
        c.grid(case, 0.4)
        bay = (rk[0] + 22, rk[1] + 22, rk[2] - 44, rk[3] - 44)
        c.cut(case, rect_box("bay", bay, D - 1.0, D + 1, c.black()))
        c.roll(case, 0.35, 8, 50)
        lip = rect_box("lip", (bay[0] - 5, bay[1] - 5, bay[2] + 10, bay[3] + 10), D - 0.1, D + 0.08, c.steel(), corner=2 * CM)
        c.grid(lip, 0.4)
        c.cut(lip, rect_box("lipcut", bay, D - 1, D + 1, c.black()))
        c.roll(lip, 0.05, 3)
        rect_box("baywall", bay, D - 1.05, D - 0.95, c.black())
        blank = c.principled("blank", (0.03, 0.032, 0.035), metallic=0.85, rough=0.42)
        unit_mat = with_print(panel_material(a["panelKind"]), pr)
        for i, r in enumerate(((bay[0], bay[1], bay[2], hy - 2 - bay[1]), (bay[0], hy + hh + 2, bay[2], bay[1] + bay[3] - hy - hh - 2))):
            if r[3] <= 2:
                continue
            bl = rect_box("blank%d" % i, r, D - 0.95, D - 0.35, blank)
            c.grid(bl, 0.4)
            if i == 0:
                # three rows of vent slots over the heat
                vw = r[2] - 172
                rows = 3
                for row in range(rows):
                    yy = r[1] + r[3] * (row + 0.5) / rows
                    xx = r[0] + 86
                    while xx + 34 <= r[0] + 86 + vw:
                        c.cut(bl, rect_box("slot", (xx, yy - 2, 34, 4), D - 1.2, D, c.black(), corner=2 * CM))
                        xx += 44
            c.roll(bl, 0.04, 2)
            for side in (0, 1):
                sx = r[0] + 23 if side == 0 else r[0] + r[2] - 23
                screw_head(st, sx, r[1] + r[3] / 2, D - 0.35, 0.16, c.chrome(), 0.4 + i + side)
        unit = rect_box("unit", (hx, hy, hw, hh), D - 0.95, D - 0.2, unit_mat, corner=3 * CM)
        c.grid(unit, 0.4)
        # the display window: a dark well with glass over it
        c.cut(unit, rect_box("well", (gx, gy, gw, gh), D - 0.5, D, c.black(), corner=8 * CM))
        c.roll(unit, 0.05, 3)
        well = rect_box("wellfloor", (gx, gy, gw, gh), D - 0.55, D - 0.5, c.principled("lcd", (0.004, 0.005, 0.005), rough=0.25, coat=1.0, coat_rough=0.02))
        for side in (0, 1):
            ex = hx + 23 if side == 0 else hx + hw - 23
            for fy in (0.2, 0.8):
                yy = hy + hh * fy
                c.cut(unit, rect_box("earslot", (ex - 12, yy - 6, 24, 12), D - 0.5, D, c.black(), corner=6 * CM))
                screw_head(st, ex, yy, D - 0.6, 0.18, c.chrome(), fy * 3 + side)
        corner_caps(st, rk, 10 * CM, 0.3, D, 34, metal)
    else:
        corner = (10 if cabinet else 12) * CM
        if cabinet:
            shell_mat = with_print(tolex(a["tolex"], a["tolexColour"], int(aid[-2:]) * 0.37), pr)
        else:
            shell_mat = with_print(c.finish_material(4, "1c1d1f"), pr)
        shell = rect_box("shell", (hx, hy, hw, hh), 0.0, D, shell_mat, corner=corner)
        c.grid(shell, 0.4)
        # the control panel, set back under the cabinet's edge
        c.cut(shell, rect_box("panelwell", (px, py, pw, ph), D - PANEL_SINK, D + 1, c.black(), corner=4 * CM))
        has_cloth = form in (0, 1, 2, 4) and a["weave"] != 7
        if has_cloth:
            c.cut(shell, rect_box("clothwell", (gx, gy, gw, gh), D - CLOTH_SINK, D + 1, c.black(), corner=6 * CM))
        if form == 3:
            vw = (gx + a["badge"][2] + 40, gy, gw - a["badge"][2] - 40, gh)
            # slots punched in the steel, the chassis dark behind them
            slot_w, slot_h, gap_x, gap_y = 30.0, 6.0, 12.0, 9.0
            cols = max(1, int((vw[2] + gap_x) / (slot_w + gap_x)))
            rows = max(1, int((vw[3] + gap_y) / (slot_h + gap_y)))
            x0 = vw[0] + vw[2] / 2 - (cols * (slot_w + gap_x) - gap_x) / 2
            y0 = vw[1] + vw[3] / 2 - (rows * (slot_h + gap_y) - gap_y) / 2
            for r in range(rows):
                for k in range(cols):
                    sx = x0 + k * (slot_w + gap_x) + ((slot_w + gap_x) * 0.5 if r % 2 else 0)
                    if sx + slot_w > vw[0] + vw[2] + 1:
                        continue
                    c.cut(shell, rect_box("vent", (sx, y0 + r * (slot_h + gap_y), slot_w, slot_h), D - 1.2, D + 1, c.black(), corner=3 * CM))
            rect_box("chassis", vw, D - 1.3, D - 1.1, c.principled("chassis", (0.006, 0.006, 0.006), rough=0.6))
        c.roll(shell, 0.42 if cabinet else 0.22, 8, 50)

        plate = rect_box("panel", (px, py, pw, ph), D - PANEL_SINK - 0.2, D - PANEL_SINK + 0.06, with_print(panel_material(a["panelKind"]), pr), corner=4 * CM)
        c.roll(plate, 0.04, 2)
        if a["panelKind"] == 3:
            # chequer plate: raised lozenges, and the black strip the controls are printed on
            strip = (px + 10, py + 12, pw - 20, ph - 24)
            yy, row = py + 6, 0
            plate_mat = panel_material(3)
            while yy < py + ph:
                xx = px + 6 + (10 if row % 2 else 0)
                while xx < px + pw:
                    if not (strip[0] - 6 < xx < strip[0] + strip[2] + 6 and strip[1] - 3 < yy < strip[1] + strip[3] + 3):
                        loz = c.box("loz", -7 * CM, 7 * CM, -1.8 * CM, 1.8 * CM, 0, 0.06, plate_mat, corner=1.7 * CM)
                        loz.location = st.at(xx, yy, D - PANEL_SINK + 0.06)
                        loz.rotation_euler[2] = 0.75 if row % 2 else -0.75
                    xx += 20
                yy += 16
                row += 1
            rect_box("strip", strip, D - PANEL_SINK + 0.04, D - PANEL_SINK + 0.08, with_print(c.principled("strip", (0.01, 0.01, 0.011), rough=0.62), pr), corner=4 * CM)

        if has_cloth:
            speakers = []
            if form == 2:
                nsp = 2 if gw > gh * 3.0 else 1
                rr = min(gh * 0.44, gw / (2 * nsp) * 0.86)
                speakers = [(gx + gw * (i + 0.5) / nsp, gy + gh / 2 + 8, rr) for i in range(nsp)]
            cp, hp = cloth_maps(st, (gx, gy, gw, gh), a["weave"], 6, speakers, work, aid)
            cloth = rect_box("cloth", (gx, gy, gw, gh), D - CLOTH_SINK - 0.3, D - CLOTH_SINK + 0.02,
                             with_print(cloth_material(cp, hp, a["weave"] in (5, 6)), pr), corner=6 * CM)
            if a["piping"]:
                piping_loop(st, (gx, gy, gw, gh), 6.0, D - CLOTH_SINK, a["piping"])
        elif a["piping"] and form != 3:
            # no cloth: a cord of piping framing the covering, inside the corner protectors
            piping_loop(st, (gx + 16, gy + 14, gw - 32, gh - 28), 9.0, D, a["piping"], thick=0.06)

        if cabinet:
            corner_caps(st, (hx, hy, hw, hh), corner, 0.3, D, 34, metal)
            strap_handle(st, a["handle"], hy, D * 0.5, leather, metal)
        else:
            bar_handle(st, a["handle"], hy, D * 0.5, c.chrome())
            for i in range(4):
                screw_head(st, hx + 11 if i % 2 == 0 else hx + hw - 11, hy + 11 if i < 2 else hy + hh - 11, D, 0.2, c.chrome(), 0.7 * i)
        # rubber feet under the bottom edge
        for fx in (hx + 64, hx + hw - 64 - 54):
            p, q = st.at(fx, hy + hh - 2, D * 0.5), st.at(fx + 54, hy + hh + 8, D * 0.5)
            ft = c.box("foot", p.x, q.x, q.y, p.y, D * 0.15, D * 0.85, c.rubber(), corner=0.15)
            c.roll(ft, 0.1, 3)

    # the raised parts of the logo
    bx, by, bw, bh = a["badge"]
    on_cloth = form in (0, 1, 2, 4) and a["weave"] != 7 and gx <= bx and by >= gy - 1 and by + bh <= gy + gh + 1
    z_face = D - CLOTH_SINK + 0.02 if on_cloth else D
    lifted = 0.0
    for k, r in enumerate(a["relief"]):
        polys = parse_path(r["path"])
        if not polys:
            continue
        colour = c.rgb(r["colour"])
        h = r["depth"] * CM * 1.4
        if r["kind"] == 3:
            # a screw through the plate
            xs = [p[0] for p in polys[0]]; ys = [p[1] for p in polys[0]]
            sx, sy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
            screw_head(st, sx, sy, z_face + lifted, (max(xs) - min(xs)) / 2 * CM, c.chrome(), sx * 0.3)
            continue
        if r["kind"] == 0:
            lum = sum(colour) / 3
            if lum < 0.05:
                m = c.principled("badge-black", (0.02, 0.021, 0.023), metallic=0.85, rough=0.38)
            else:
                # polished, but a face looking at the player mirrors the dark room: a little satin keeps it bright
                m = c.principled("badge-metal", tuple(min(1.0, v * 1.02) for v in colour), metallic=0.8, rough=0.26, coat=0.6, coat_rough=0.05)
        elif r["kind"] == 1:
            m = c.principled("badge-moulded", tuple(v * 0.72 for v in colour), rough=0.3, coat=0.5, coat_rough=0.1)
        else:
            m = c.principled("badge-enamel", tuple(v * 0.8 for v in colour), rough=0.25, coat=1.0, coat_rough=0.04)
        m = with_print(m, pr)
        base = z_face + (lifted if r["kind"] in (1, 2) else 0.0) + r["lift"] * CM
        raised_shape(st, "badge%d" % k, polys, base, h, m, min(0.06, h * 0.35))
        if r["kind"] == 0:
            lifted = max(lifted, h * 0.6)

    out = os.path.join(out_dir, "amp_" + slug(a["name"]) + ".png")
    st.render(out)
    return out


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    gear_dir, out_dir = args[0], args[1]
    only, samples, scale = None, 128, 2.0
    for i, a in enumerate(args):
        if a == "--only": only = set(args[i + 1].split(","))
        if a == "--samples": samples = int(args[i + 1])
        if a == "--scale": scale = float(args[i + 1])
    os.makedirs(out_dir, exist_ok=True)
    gear = json.load(open(os.path.join(gear_dir, "gear.json")))
    for a in gear["amps"]:
        if only and a["id"] not in only:
            continue
        print("amp", a["id"], flush=True)
        build(a, gear_dir, out_dir, samples, scale)


if __name__ == "__main__":
    main()
