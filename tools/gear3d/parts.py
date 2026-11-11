# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Photograph the parts that move: knobs, and the caps of the sliders.

    python parts.py <output folder> [--only name,name] [--samples n] [--px n]

Each knob stands on a clear floor that only keeps its shadow, lit by the same room as the gear.
A round knob is one picture: the app draws its pointer line on top as it turns. A chicken-head
knob is not round, so it gets a strip of pictures across its travel.

Every picture is a square whose middle is the foot of the knob's shaft, four knob radii wide.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c
from common import CM

R_NOMINAL = 32.0                 # the knob's radius in the picture, in nominal pixels
CANVAS = R_NOMINAL * 4.0
FRAMES = 65                      # chicken head: pictures across the 270 degrees of travel
START, END = math.pi * 1.25, math.pi * 2.75


def stage(px, samples):
    return c.Stage((0.0, 0.0, CANVAS, CANVAS), 0.0, scale=px / CANVAS, samples=samples)


def knob_modern(r):
    m = c.principled("satin", (0.008, 0.008, 0.009), rough=0.36)
    return [c.lathe("knob", [(r, 0.02 * r), (r, 0.9 * r), (0.97 * r, 0.97 * r), (0.9 * r, 1.0 * r), (0.4 * r, 1.01 * r), (0.0001, 1.01 * r)], m, seg=128)]


def knob_pedal(r):
    # the everyday stomp box knob: a fluted grip and a flat top
    prof = [(0.93 * r, 0.04 * r), (r, 0.1 * r), (r, 0.3 * r), (0.96 * r, 0.38 * r),
            (0.93 * r, 0.44 * r), (0.93 * r, 1.12 * r), (0.9 * r, 1.22 * r), (0.84 * r, 1.27 * r),
            (0.74 * r, 1.29 * r), (0.35 * r, 1.305 * r), (0.0001, 1.31 * r)]
    return [c.lathe("knob", prof, c.bakelite(), seg=224, flutes=28, depth=0.055, flute_rings=(4, 6))]


def knob_skirted(r):
    # amp knob: a flared skirt, a fluted grip, a spun metal disc in the top
    skirt = [(r, 0.0), (r, 0.1 * r), (0.97 * r, 0.16 * r), (0.66 * r, 0.42 * r), (0.6 * r, 0.46 * r)]
    grip = [(0.6 * r, 0.44 * r), (0.6 * r, 1.12 * r), (0.57 * r, 1.2 * r), (0.5 * r, 1.24 * r), (0.42 * r, 1.25 * r)]
    disc = [(0.42 * r, 1.2 * r), (0.42 * r, 1.265 * r), (0.39 * r, 1.285 * r), (0.2 * r, 1.29 * r), (0.0001, 1.29 * r)]
    spun = c.principled("spun", (0.62, 0.63, 0.65), metallic=1.0, rough=0.24)
    n = c.Nodes(spun)                                   # turned on a lathe: fine rings
    sep = n.new("ShaderNodeSeparateXYZ")
    n.link(n.coord.outputs["Object"], sep.inputs["Vector"])
    rad = n.math("SQRT", n.math("ADD", n.math("MULTIPLY", sep.outputs["X"], sep.outputs["X"]), n.math("MULTIPLY", sep.outputs["Y"], sep.outputs["Y"])))
    rings = n.math("SINE", n.math("MULTIPLY", rad, 900.0 / r * 0.35))
    n.link(n.bump(rings, 0.12, 0.002), n.bsdf.inputs["Normal"])
    return [c.lathe("skirt", skirt, c.bakelite(), seg=160),
            c.lathe("grip", grip, c.bakelite(), seg=192, flutes=20, depth=0.065, flute_rings=(0, 2)),
            c.lathe("disc", disc, spun, seg=96)]


def knob_dome(r):
    # a smooth dome of spun aluminium
    prof = [(r, 0.0), (r, 0.5 * r)]
    for i in range(1, 13):
        a = (math.pi / 2) * i / 12
        prof.append((max(0.0001, r * math.cos(a)), 0.5 * r + 0.42 * r * math.sin(a)))
    alu = c.principled("alu", (0.72, 0.73, 0.75), metallic=1.0, rough=0.2)
    return [c.lathe("knob", prof, alu, seg=128)]


def dome_height(rho):            # how high the dome is at rho (in radii) from its middle
    return 0.5 + 0.42 * math.sqrt(max(0.0, 1.0 - rho * rho))


def knob_hat(r, cap_hex):
    # top hat: a ribbed black skirt with a brushed metal cap pressed into the crown
    skirt = [(r, 0.0), (r, 0.14 * r), (0.96 * r, 0.2 * r), (0.6 * r, 0.52 * r), (0.56 * r, 0.56 * r)]
    crown = [(0.56 * r, 0.5 * r), (0.56 * r, 1.12 * r), (0.54 * r, 1.17 * r)]
    capc = c.rgb(cap_hex)
    dark = c.luminance(capc) < 0.05
    metal = c.principled("cap", (0.3, 0.31, 0.33) if dark else capc, metallic=1.0, rough=0.3)
    cap = [(0.54 * r, 1.1 * r), (0.54 * r, 1.19 * r), (0.5 * r, 1.22 * r), (0.2 * r, 1.23 * r), (0.0001, 1.23 * r)]
    return [c.lathe("skirt", skirt, c.bakelite(), seg=192, flutes=24, depth=0.07, flute_rings=(1, 3)),
            c.lathe("crown", crown, c.bakelite(), seg=96),
            c.lathe("cap", cap, metal, seg=96)]


def knob_chicken(r, angle):
    """A chicken-head pointer: a round base and a long tapered beak, moulded in one glossy piece."""
    import bmesh
    m = c.principled("chicken", (0.006, 0.006, 0.006), rough=0.2)
    n = c.Nodes(m)                                      # the cream line let into the ridge
    sep = n.new("ShaderNodeSeparateXYZ")
    n.link(n.coord.outputs["Object"], sep.inputs["Vector"])
    on_line = n.math("MULTIPLY", n.math("LESS_THAN", n.math("ABSOLUTE", sep.outputs["X"]), 0.05 * r),
                     n.math("MULTIPLY", n.math("GREATER_THAN", sep.outputs["Y"], 0.4 * r), n.math("GREATER_THAN", sep.outputs["Z"], 0.6 * r)))
    n.link(n.mix(on_line, (0.006, 0.006, 0.006), c.rgb("f1ead8")), n.bsdf.inputs["Base Color"])
    base = c.lathe("base", [(0.9 * r, 0.0), (0.9 * r, 0.3 * r), (0.86 * r, 0.42 * r), (0.7 * r, 0.5 * r), (0.0001, 0.52 * r)], m, seg=96)

    def quad(p0, p1, p2, steps=14):
        return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1])
                for t in [i / steps for i in range(steps)]]
    # the outline seen from above, pointing along +y
    pts = quad((0.0, 1.02), (0.34, 0.2), (0.26, -0.55)) + quad((0.26, -0.55), (0.0, -0.72), (-0.26, -0.55)) + quad((-0.26, -0.55), (-0.34, 0.2), (0.0, 1.02))
    bm = bmesh.new()
    low = [bm.verts.new((x * r, y * r, 0.3 * r)) for x, y in pts]
    # the ridge: narrower, and falling toward the tip
    def top_z(y):
        return (1.02 - 0.2 * max(0.0, y) ** 1.5) * r
    high = [bm.verts.new((x * r * 0.42, y * r * 0.95, top_z(y))) for x, y in pts]
    k = len(pts)
    for i in range(k):
        bm.faces.new((low[i], low[(i + 1) % k], high[(i + 1) % k], high[i]))
    bm.faces.new(high)
    bm.faces.new(list(reversed(low)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = c.bpy.data.meshes.new("head")
    bm.to_mesh(me)
    bm.free()
    head = c.link(me, "head", m)
    mod = head.modifiers.new("round", "BEVEL")
    mod.width, mod.segments, mod.limit_method, mod.angle_limit = 0.07 * r, 5, "ANGLE", math.radians(25)
    head.modifiers.new("normals", "WEIGHTED_NORMAL")
    for ob in (base, head):
        ob.rotation_euler[2] = -angle
    return [base, head]


def fader_cap():
    """The cap of a slider, 18 by 30 of the app's pixels: a ribbed block with a white line across its middle."""
    w, l = 18 * CM, 30 * CM
    cap = c.box("cap", -w / 2, w / 2, -l / 2, l / 2, 0.06, 0.58, c.bakelite(), corner=0.06, corner_segments=4, taper=(0.86, 0.9))
    c.roll(cap, 0.035, 4)
    obs = [cap]
    for i in (-3, -2, -1, 1, 2, 3):                      # finger ribs either side of the line
        rib = c.box("rib", -w * 0.36, w * 0.36, i * l * 0.115 - 0.018, i * l * 0.115 + 0.018, 0.56, 0.615, c.bakelite())
        c.roll(rib, 0.012, 2)
        obs.append(rib)
    obs.append(c.box("line", -w * 0.38, w * 0.38, -0.022, 0.022, 0.57, 0.6, c.principled("line", c.rgb("f1ead8"), rough=0.5)))
    return obs


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = args[0]
    only, samples, px, first = None, 128, 384, 0
    for i, a in enumerate(args):
        if a == "--from": first = int(args[i + 1])      # carry on a strip that was cut short
        if a == "--only": only = set(args[i + 1].split(","))
        if a == "--samples": samples = int(args[i + 1])
        if a == "--px": px = int(args[i + 1])
    os.makedirs(out, exist_ok=True)
    r = R_NOMINAL * CM
    round_knobs = {
        "knob-modern": lambda: knob_modern(r),
        "knob-pedal": lambda: knob_pedal(r),
        "knob-skirted": lambda: knob_skirted(r),
        "knob-dome": lambda: knob_dome(r),
        "knob-hat-16181a": lambda: knob_hat(r, "16181a"),
        "knob-hat-d8b04a": lambda: knob_hat(r, "d8b04a"),
    }
    for name, make in round_knobs.items():
        if only and name not in only:
            continue
        print("part", name, flush=True)
        st = stage(px, samples)
        make()
        st.render(os.path.join(out, name + ".png"))
    if not only or "knob-chicken" in only:
        for f in range(first, FRAMES):
            print("part knob-chicken", f, flush=True)
            st = stage(px, samples)
            knob_chicken(r, START + (END - START) * f / (FRAMES - 1))
            st.render(os.path.join(out, "_chicken", "%02d.png" % f))
    if not only or "fader-cap" in only:
        print("part fader-cap", flush=True)
        st = stage(px, samples)
        fader_cap()
        st.render(os.path.join(out, "fader_cap.png"))


if __name__ == "__main__":
    main()
