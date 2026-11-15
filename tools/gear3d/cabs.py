# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Build and photograph every speaker cabinet.

    blender -b -P cabs.py -- <gear folder written by `Riffhouse --snapshots`> <output folder> [--only id,id] [--samples n] [--scale s]

Seen from the front like the amps (amps.py): the cabinet lies on the wall face up, height is
distance out toward the player. The app says where the box, the baffle and every speaker sit
(gear.json "cabs") and hands over its logo as outlines. The speakers are mounted on the baffle
board, the grille cloth is stretched in front of them, and the light gets through the gaps in the
weave, so the cones read through the cloth the way they do on a real cab. The app lays the mic on
top, and on the IR cab the words on the display.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402
import amps  # noqa: E402
from common import CM  # noqa: E402

D = 2.4             # the front of the cabinet, out from the wall, cm
CLOTH_Z = D - 0.34  # the grille cloth, stretched a little behind the cabinet's edge
BAFFLE_Z = D - 0.95 # the face of the baffle board the speakers are screwed to
BACK_Z = 0.35       # the inside of the back panel, black


def ring_lathe(name, profile, material, seg=128, at=(0, 0, 0)):
    """A profile of (radius, height) turned round the z axis, open at both ends."""
    import bmesh
    bm = bmesh.new()
    rings = []
    for r, z in profile:
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * s / seg), r * math.sin(2 * math.pi * s / seg), z)) for s in range(seg)])
    for a, b in zip(rings, rings[1:]):
        for s in range(seg):
            bm.faces.new((a[s], a[(s + 1) % seg], b[(s + 1) % seg], b[s]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # every face looks out toward the player
    for f in bm.faces:
        if f.normal.z < -0.05:
            f.normal_flip()
    me = c.bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = c.link(me, name, material)
    ob.location = at
    return ob


def speaker_materials(frame_hex, alloy):
    fr = c.rgb(frame_hex)
    frame = c.principled("spk-frame", tuple(v * 0.85 for v in fr), metallic=0.75, rough=0.38, coat=0.3)
    gasket = c.principled("spk-gasket", (0.022, 0.022, 0.021), rough=0.92)
    n = c.Nodes(gasket)
    n.link(n.bump(n.noise(180.0, 3.0), 0.3, 0.01), n.bsdf.inputs["Normal"])
    edge = c.principled("spk-edge", (0.014, 0.013, 0.012), rough=0.6, coat=0.25, coat_rough=0.3)
    cone = c.principled("spk-cone", (0.03, 0.026, 0.021), rough=0.88)
    n = c.Nodes(cone)
    # pressed pulp: long fibres and fine grain
    n.link(n.bump(n.math("ADD", n.noise(70.0, 4.0), n.math("MULTIPLY", n.noise(400.0, 2.0), 0.5)), 0.35, 0.01), n.bsdf.inputs["Normal"])
    if alloy:
        cap = c.principled("spk-cap", (0.62, 0.63, 0.64), metallic=1.0, rough=0.3)
    else:
        cap = c.principled("spk-cap", (0.018, 0.016, 0.014), rough=0.95)
        n = c.Nodes(cap)
        n.link(n.bump(n.noise(260.0, 2.0), 0.4, 0.01), n.bsdf.inputs["Normal"])
    return frame, gasket, edge, cone, cap


def speaker(st, rect, mats, bolts_metal, seed):
    """A loudspeaker on the baffle: pressed steel rim, fibre gasket, bolts, two rolls of edge, the cone, the cap."""
    x, y, w, h = rect
    r = w * 0.5 * CM
    at = st.at(x + w / 2, y + h / 2, BAFFLE_Z)
    frame, gasket, edge, cone, cap = mats

    def prof(points):
        return [(r * a, r * b) for a, b in points]

    ring_lathe("frame", prof([(0.85, -0.02), (0.86, 0.035), (0.94, 0.035), (0.955, 0.05), (0.985, 0.05), (1.0, 0.03), (1.0, 0.0), (1.02, -0.01)]), frame, at=at)
    ring_lathe("gasket", prof([(0.845, 0.05), (0.85, 0.08), (0.93, 0.08), (0.935, 0.05)]), gasket, at=at)
    rolls = []
    for k in range(25):
        t = k / 24
        rolls.append((0.845 - 0.11 * t, 0.045 + 0.04 * abs(math.sin(math.pi * 2 * t))))
    ring_lathe("edge", prof(rolls), edge, at=at)
    cone_pts = []
    for k in range(25):
        t = k / 24
        rad = 0.735 - 0.5 * t
        z = 0.045 - 0.36 * t ** 0.85
        # pressed ribs near the edge
        z += 0.006 * math.sin(t * math.pi * 10) * (1 - t)
        cone_pts.append((rad, z))
    ring_lathe("cone", prof(cone_pts), cone, at=at)
    cap_pts = [(0.235 * math.cos(a), -0.315 + 0.13 * math.sin(a)) for a in np.linspace(0.0, math.pi / 2 - 0.02, 14)]
    cap_pts.append((0.0005, -0.315 + 0.13))
    ring_lathe("cap", prof(cap_pts), cap, at=at)
    n_bolts = 8 if w * 0.5 > 70 else 4
    for i in range(n_bolts):
        a = 2 * math.pi * i / n_bolts + (math.pi / 4 if n_bolts == 4 else 0)
        bx, by = x + w / 2 + 0.89 * w / 2 * math.cos(a), y + h / 2 + 0.89 * w / 2 * math.sin(a)
        amps.screw_head(st, bx, by, BAFFLE_Z + 0.08 * r, max(0.07, 0.045 * r), bolts_metal, seed * 1.7 + i)


def cloth_with_alpha(st, rect, weave, show, speakers, work, cid, bend=None):
    """The cloth's colour and height (amps.cloth_maps), and how solid it is: light gets through the weave."""
    cp, hp = amps.cloth_maps(st, rect, weave, 6, [] if weave == 5 else speakers, work, cid)
    col = np.asarray(Image.open(cp)).astype(np.float32) / 255.0
    hgt = np.asarray(Image.open(hp)).astype(np.float32) / 255.0
    H, W = hgt.shape
    ax, ay, aw, ah = st.area
    s = W / aw
    x0, y0 = int((rect[0] - ax) * s), int((rect[1] - ay) * s)
    x1, y1 = int((rect[0] + rect[2] - ax) * s), int((rect[1] + rect[3] - ay) * s)
    inside = np.zeros_like(hgt, bool)
    inside[y0:y1, x0:x1] = True
    if weave == 5:      # punched steel: the holes are open
        alpha = np.clip(hgt * 1.0, 0, 1)
    elif weave == 6:    # a metal grid over thin black cloth
        alpha = 1.0 - show * (1.0 - np.clip(hgt * 1.6, 0, 1))
    else:               # woven: the gaps between threads let a little through
        q = np.quantile(hgt[inside], min(0.95, show * 1.15))
        alpha = np.clip((hgt - q * 0.6) / max(0.05, q * 0.8), 0, 1)
        alpha = 0.18 + 0.82 * alpha
    if bend is not None:
        # a slant cab: the cloth above the bend leans back and faces the light a little more
        by = int((bend - ay) * s)
        col[y0:by, x0:x1] *= 1.07
        col[by:by + max(1, int(1.4 * s)), x0:x1] *= 0.55
        col[by + max(1, int(1.4 * s)):by + max(2, int(2.4 * s)), x0:x1] *= 1.15
    alpha = np.where(inside, alpha, 0.0)
    Image.fromarray((np.clip(col, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(cp)
    ap = os.path.join(work, cid + "-cloth-a.png")
    Image.fromarray((alpha * 255 + 0.5).astype(np.uint8), "L").save(ap)
    return cp, hp, ap


def see_through(m, alpha_path):
    """Let the light through where the alpha picture is dark."""
    nt = m.node_tree
    n = c.Nodes(m)
    a = n.image(alpha_path, colorspace="Non-Color")
    out = nt.nodes["Material Output"]
    mix = nt.nodes.new("ShaderNodeMixShader")
    clear = nt.nodes.new("ShaderNodeBsdfTransparent")
    nt.links.new(a.outputs["Color"], mix.inputs["Fac"])
    nt.links.new(clear.outputs[0], mix.inputs[1])
    nt.links.new(n.bsdf.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def build(k, gear_dir, out_dir, samples, scale):
    cid, model = k["id"], k["model"]
    work = os.path.join(out_dir, "_work")
    os.makedirs(work, exist_ok=True)
    st = c.Stage(k["area"], D, scale=scale, samples=samples, floor=True)
    pr = os.path.join(gear_dir, cid + "-print.png")
    bx, by, bw, bh = k["box"]
    fx, fy, fw, fh = k["baffle"]
    weave = k["weave"]
    metal = c.principled("corner", (0.72, 0.72, 0.73), metallic=1.0, rough=0.2)
    leather = c.principled("leather", (0.018, 0.014, 0.012), rough=0.5, coat=0.15)
    ln = c.Nodes(leather)
    ln.link(ln.bump(ln.voronoi(40.0, "SMOOTH_F1"), 0.2, 0.01), ln.bsdf.inputs["Normal"])

    def rect_box(name, r, z0, z1, mat, corner=0.0):
        p, q = st.at(r[0], r[1], z1), st.at(r[0] + r[2], r[1] + r[3], z1)
        return c.box(name, p.x, q.x, q.y, p.y, z0, z1, mat, corner=corner)

    # the box, covered all round, open at the front where the baffle sits
    corner = 10 * CM
    shell = rect_box("shell", (bx, by, bw, bh), 0.0, D, amps.with_print(amps.tolex(k["tolex"], k["tolexColour"], model * 0.37), pr), corner=corner)
    c.grid(shell, 0.4)
    c.cut(shell, rect_box("well", (fx, fy, fw, fh), BACK_Z, D + 1, c.black(), corner=4 * CM))
    c.roll(shell, 0.42, 8, 50)
    rect_box("back", (fx, fy, fw, fh), BACK_Z - 0.05, BACK_Z, c.principled("inside", (0.004, 0.004, 0.004), rough=0.9))

    # the baffle: black painted ply, a hole for every speaker
    open_front = weave == 7
    zb = BAFFLE_Z if not open_front else D - 0.42
    paint = c.principled("baffle", (0.006, 0.006, 0.0065), rough=0.78)
    pn = c.Nodes(paint)
    pn.link(pn.bump(pn.noise(140.0, 3.0), 0.12, 0.008), pn.bsdf.inputs["Normal"])
    baffle = rect_box("baffle", (fx, fy, fw, fh), zb - 0.3, zb, paint, corner=4 * CM)
    c.grid(baffle, 0.5)
    for i, (sx, sy, sw, sh) in enumerate(k["speakers"]):
        hole = c.prism("hole%d" % i, 96, sw * 0.5 * CM * 0.86, zb - 1.0, zb + 0.5, c.black(), at=st.at(sx + sw / 2, sy + sh / 2, zb))
        hole.location.z = 0.0
        c.cut(baffle, hole)
    if model == 7:
        # the port: a slot through the board under the big driver
        c.cut(baffle, rect_box("port", (fx + fw * 0.22, fy + fh - fh * 0.11, fw * 0.56, fh * 0.07), zb - 1.0, zb + 0.5, c.black(), corner=fh * 0.035 * CM))
    mats = speaker_materials(k["frame"], model == 6)
    # bolts and baffle screws: black under a cloth, bright on the open IR cab
    bolt_metal = c.principled("bolt", (0.08, 0.08, 0.085), metallic=0.9, rough=0.35) if not open_front else c.chrome()
    for i, sp in enumerate(k["speakers"]):
        _speaker_at(st, sp, mats, bolt_metal, model * 16 + i, zb)
    if model == 6:
        # a horn for the top end between the four
        hw_, hh_ = fw * 0.17, fw * 0.085
        hr = (fx + fw / 2 - hw_ / 2, fy + fh / 2 - hh_ / 2, hw_, hh_)
        horn = rect_box("horn", hr, zb, zb + 0.18, c.principled("horn", (0.03, 0.031, 0.033), rough=0.35), corner=3 * CM)
        c.cut(horn, rect_box("throat", (hr[0] + 4, hr[1] + 3, hr[2] - 8, hr[3] - 6), zb - 0.6, zb + 1, c.black(), corner=3 * CM))
        rect_box("throatback", (hr[0] + 4, hr[1] + 3, hr[2] - 8, hr[3] - 6), zb - 0.62, zb - 0.58, c.principled("dark", (0.002, 0.002, 0.002), rough=0.9))
        c.roll(horn, 0.04, 3)
    if open_front:
        for i in range(4):
            amps.screw_head(st, fx + 9 if i % 2 == 0 else fx + fw - 9, fy + 9 if i < 2 else fy + fh - 9, zb, 0.13, c.chrome(), 0.5 + 0.9 * i)

    # what covers the speakers
    z_face = D
    if not open_front:
        bend = fy + fh * 0.46 if model == 2 else None
        spk = [(sx + sw / 2, sy + sh / 2, sw / 2) for sx, sy, sw, sh in k["speakers"]]
        cp, hp, ap = cloth_with_alpha(st, (fx, fy, fw, fh), weave, k["show"], spk, work, cid, bend)
        metal_sheet = weave in (5, 6)
        cm = amps.cloth_material(cp, hp, metal_sheet)
        if weave == 5:
            cm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.45
        see_through(amps.with_print(cm, pr), ap)
        rect_box("cloth", (fx, fy, fw, fh), CLOTH_Z - 0.02, CLOTH_Z, cm, corner=4 * CM)
        z_face = CLOTH_Z
        if weave == 5:
            # the sheet is screwed down round its edge
            for i in range(6):
                amps.screw_head(st, fx + 8 + (fw - 16) * (i % 3) * 0.5, fy + 8 if i < 3 else fy + fh - 8, CLOTH_Z, 0.12, c.chrome(), 0.7 * i)
        if k["piping"]:
            amps.piping_loop(st, (fx + 2.5, fy + 2.5, fw - 5, fh - 5), 4.0, CLOTH_Z, k["piping"], thick=0.07)

    if model == 8:
        # the display that names the loaded IR: a plate on the board, a dark window in it
        plate = k["irPlate"]
        win = k["irWindow"]
        pm = c.principled("irplate", (0.03, 0.032, 0.034), metallic=0.85, rough=0.38)
        pl = rect_box("irplate", plate, zb, zb + 0.22, pm, corner=5 * CM)
        c.grid(pl, 0.4)
        c.cut(pl, rect_box("irwin", win, zb + 0.08, zb + 1, c.black(), corner=8 * CM))
        c.roll(pl, 0.05, 3)
        rect_box("irglass", win, zb + 0.06, zb + 0.09, c.principled("lcd", (0.004, 0.005, 0.005), rough=0.25, coat=1.0, coat_rough=0.02), corner=8 * CM)
        for xx in (plate[0] + 11, plate[0] + plate[2] - 11):
            amps.screw_head(st, xx, plate[1] + plate[3] / 2, zb + 0.22, 0.13, c.chrome(), xx * 0.37)

    if model == 4:
        # tilt-back legs folded against the sides, seen edge on
        chrome = c.chrome()
        for lx in (bx - 5.5, bx + bw + 1.0):
            leg = rect_box("leg", (lx, by + bh * 0.2, 4.5, bh * 0.62), 0.5, D - 0.5, chrome, corner=2.2 * CM)
            c.roll(leg, 0.08, 4)
    if k["handle"]:
        amps.strap_handle(st, k["handle"], by, D * 0.5, leather, metal)
    if model != 5:
        amps.corner_caps(st, (bx, by, bw, bh), corner, 0.3, D, min(32.0, bw * 0.075), metal)
    for fxx in (bx + bw * 0.12, bx + bw * 0.88 - 54):
        p, q = st.at(fxx, by + bh - 2, D * 0.5), st.at(fxx + 54, by + bh + 8, D * 0.5)
        ft = c.box("foot", p.x, q.x, q.y, p.y, D * 0.15, D * 0.85, c.rubber(), corner=0.15)
        c.roll(ft, 0.1, 3)

    # the amp's name on the cloth
    lifted = 0.0
    for n_, r in enumerate(k["relief"]):
        polys = amps.parse_path(r["path"])
        if not polys:
            continue
        colour = c.rgb(r["colour"])
        h = r["depth"] * CM * 1.4
        if r["kind"] == 3:
            xs = [p[0] for p in polys[0]]; ys = [p[1] for p in polys[0]]
            sx, sy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
            amps.screw_head(st, sx, sy, z_face + lifted, (max(xs) - min(xs)) / 2 * CM, c.chrome(), sx * 0.3)
            continue
        if r["kind"] == 0:
            if sum(colour) / 3 < 0.05:
                m = c.principled("badge-black", (0.02, 0.021, 0.023), metallic=0.85, rough=0.38)
            else:
                m = c.principled("badge-metal", tuple(min(1.0, v * 1.02) for v in colour), metallic=0.8, rough=0.26, coat=0.6, coat_rough=0.05)
        elif r["kind"] == 1:
            m = c.principled("badge-moulded", tuple(v * 0.72 for v in colour), rough=0.3, coat=0.5, coat_rough=0.1)
        else:
            m = c.principled("badge-enamel", tuple(v * 0.8 for v in colour), rough=0.25, coat=1.0, coat_rough=0.04)
        m = amps.with_print(m, pr)
        base = z_face + (lifted if r["kind"] in (1, 2) else 0.0) + r["lift"] * CM
        amps.raised_shape(st, "badge%d" % n_, polys, base, h, m, min(0.06, h * 0.35))
        if r["kind"] == 0:
            lifted = max(lifted, h * 0.6)

    out = os.path.join(out_dir, k["name"] + ".png")
    st.render(out)
    return out


def _speaker_at(st, rect, mats, bolt_metal, seed, zb):
    global BAFFLE_Z
    keep = BAFFLE_Z
    BAFFLE_Z = zb
    try:
        speaker(st, rect, mats, bolt_metal, seed)
    finally:
        BAFFLE_Z = keep


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
    for k in gear["cabs"]:
        if only and k["id"] not in only:
            continue
        print("cab", k["id"], flush=True)
        build(k, gear_dir, out_dir, samples, scale)


if __name__ == "__main__":
    main()
