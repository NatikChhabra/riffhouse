# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Photograph the cab page's "mic, seen from above" picture: a cutaway of a speaker and its board.

    blender -b -P diagram.py -- <output folder> [--samples n] [--scale s]

Two pictures, in the nominal pixels of MicMap::plan() (Modules.cpp), origin at the top left of its
area, 392 wide, the speaker's axis at x = 176 and the baffle at y = 74:

    mic_diagram   the board sawn through, and half of the speaker: basket, paper cone, dust cap, magnet
    mic_probe     the microphone side on, nose up, to be moved across the picture by the app

The speaker is a real half-solid cut down its axis, lit like the rest of the gear, so the cut face
and the hollow of the cone read as depth. Cut at z = ZC; everything behind it runs back to the wall.
"""
import math
import os
import sys

import bmesh

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402
from common import CM  # noqa: E402
from mathutils import Vector  # noqa: E402

CX, BAFFLE = 176.0, 74.0     # where MicMap::plan() puts the speaker's axis and the baffle
CONE_R, CAP_R = 150.0, 37.5  # its widest plan: min(150, 392 * 0.4) and a quarter of that
APEX = BAFFLE - 46.0
ZC = CONE_R * CM + 0.3       # the sawn face, out from the wall
BOARD = 8.0                  # plywood, nominal px thick


def shell_half(name, profile, material, thick, seg=160):
    """A profile of (radius, height toward the mic) turned round the speaker's axis, sawn in half, given thickness."""
    bm = bmesh.new()
    rings = []
    for r, h in profile:
        row = []
        for s in range(seg + 1):
            a = math.pi * s / seg          # half a turn: the half behind the sawn face
            row.append(bm.verts.new((r * math.cos(a), -r * math.sin(a), h)))
        rings.append(row)
    for a, b in zip(rings, rings[1:]):
        for s in range(seg):
            bm.faces.new((a[s], a[s + 1], b[s + 1], b[s]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = c.bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = c.link(me, name, material)
    sol = ob.modifiers.new("thick", "SOLIDIFY")
    sol.thickness, sol.offset = thick, 0.0
    return ob


def put(ob, st, x, y):
    """Stand a part built round z (z toward the mic = down the picture) on the speaker's axis at nominal (x, y)."""
    ob.rotation_euler = (math.pi / 2, 0, 0)       # local z -> down the picture; the half kept lies behind the sawn face
    p = st.at(x, y, ZC)
    ob.location = p
    return ob


def plywood():
    m = c.principled("ply", (0.2, 0.13, 0.07), rough=0.62)
    n = c.Nodes(m)
    mp = n.new("ShaderNodeMapping")
    n.link(n.coord.outputs["Object"], mp.inputs["Vector"])
    wave = n.new("ShaderNodeTexWave", wave_type="BANDS", bands_direction="Y")
    wave.inputs["Scale"].default_value = 24.0
    wave.inputs["Distortion"].default_value = 1.4
    n.link(mp.outputs["Vector"], wave.inputs["Vector"])
    grain = n.noise(30.0, 4.0, stretch=(14.0, 1.0, 1.0))
    f = n.math("ADD", n.math("MULTIPLY", wave.outputs["Fac"], 0.7), n.math("MULTIPLY", grain, 0.5))
    n.link(n.ramp(f, [(0.25, (0.06, 0.035, 0.016)), (0.55, (0.2, 0.125, 0.06)), (0.85, (0.32, 0.21, 0.1))]), n.bsdf.inputs["Base Color"])
    n.link(n.bump(f, 0.25, 0.01), n.bsdf.inputs["Normal"])
    return m


def diagram(out, samples, scale):
    area = (0.0, -4.0, 392.0, 132.0)
    st = c.Stage(area, ZC, scale=scale, samples=samples, floor=True)
    steel = c.principled("basket", (0.2, 0.21, 0.22), metallic=0.9, rough=0.42)
    paper = c.principled("paper", (0.13, 0.105, 0.08), rough=0.9)
    pn = c.Nodes(paper)
    pn.link(pn.bump(pn.math("ADD", pn.noise(70.0, 4.0), pn.noise(400.0, 2.0)), 0.3, 0.01), pn.bsdf.inputs["Normal"])
    cap_m = c.principled("cap", (0.018, 0.016, 0.014), rough=0.9)
    magnet = c.principled("magnet", (0.07, 0.07, 0.075), metallic=0.8, rough=0.5)
    edge = c.principled("edge", (0.018, 0.017, 0.016), rough=0.55, coat=0.2)
    s = CM
    # profile height h: toward the mic is positive, so the apex sits behind the baffle at -46 px
    cone = [(CONE_R * s, 0.0)]
    for k in range(1, 25):
        t = k / 24
        r = CONE_R - (CONE_R - CAP_R) * t
        h = -46.0 * (t ** 1.7)          # the paper falls away slowly, then steeply: a curved horn
        cone.append((r * s, h * s))
    put(shell_half("cone", cone, paper, 0.06), st, CX, BAFFLE)
    dome = [(CAP_R * math.cos(a) * s, (-46.0 + CAP_R * 0.55 * math.sin(a)) * s) for a in [math.pi / 2 - i * (math.pi / 2) / 14 for i in range(15)]]
    dome = [(max(r, 0.0001), h) for r, h in reversed(dome)]
    put(shell_half("cap", dome, cap_m, 0.05), st, CX, BAFFLE)
    put(shell_half("surround", [(CONE_R * s - 0.35 + 0.7 * i / 12, 0.18 * math.sin(math.pi * i / 12)) for i in range(13)], edge, 0.05), st, CX, BAFFLE)
    # the pressed steel basket and the magnet on its back
    basket = [((CONE_R + 3.0) * s, -5.0 * s), ((CAP_R * 0.95 + 3.0) * s, (-46.0 - 14.0) * s)]
    put(shell_half("basket", basket, steel, 0.05), st, CX, BAFFLE)
    mag = [(CAP_R * 0.95 * s, (-46.0 - 6.0) * s), (CAP_R * 0.95 * s, (-46.0 - 20.0) * s), (0.0001, (-46.0 - 20.0) * s)]
    m = shell_half("magnet", mag, magnet, 0.3)
    put(m, st, CX, BAFFLE)
    st.render(os.path.join(out, "mic_diagram.png"))


def probe(out, samples, scale):
    area = (-22.0, -10.0, 44.0, 84.0)
    st = c.Stage(area, 3.0, scale=scale, samples=samples, floor=False)
    body = c.principled("mic-body", (0.018, 0.018, 0.02), metallic=0.4, rough=0.38, coat=0.35, coat_rough=0.2)
    mesh = c.principled("mic-mesh", (0.5, 0.5, 0.52), metallic=1.0, rough=0.4)
    n = c.Nodes(mesh)
    n.link(n.bump(n.voronoi(120.0, "F1"), 0.5, 0.01), n.bsdf.inputs["Normal"])
    s = CM
    # radius, height along the axis (nose up the page is -y, so heights run down the picture)
    head = [(0.0001, 0.0), (5.2 * s, 0.9 * s), (8.0 * s, 4.0 * s), (9.5 * s, 9.0 * s), (9.5 * s, 14.0 * s), (8.6 * s, 15.0 * s)]
    grille = c.lathe("grille", head, mesh, seg=64)
    ring = c.lathe("ring", [(8.6 * s, 14.5 * s), (9.6 * s, 14.5 * s), (9.6 * s, 17.0 * s), (8.4 * s, 17.0 * s)], body, seg=64)
    tube = c.lathe("tube", [(7.0 * s, 17.0 * s), (6.6 * s, 30.0 * s), (6.0 * s, 62.0 * s), (0.0001, 62.0 * s)], body, seg=64)
    for ob in (grille, ring, tube):
        ob.rotation_euler = (math.pi / 2, 0, 0)
        ob.location = st.at(0.0, 0.0, 3.0)
    st.render(os.path.join(out, "mic_probe.png"))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = args[0]
    samples, scale = 128, 2.0
    for i, a in enumerate(args):
        if a == "--samples": samples = int(args[i + 1])
        if a == "--scale": scale = float(args[i + 1])
    os.makedirs(out, exist_ok=True)
    diagram(out, samples, scale)
    probe(out, samples, scale)


if __name__ == "__main__":
    main()
