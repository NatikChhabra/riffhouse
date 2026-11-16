# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Photograph the tuner's hardware: a stage tuner's face with a note window and a needle window.

    blender -b -P tuner.py -- <output folder> [--samples n] [--scale s]

Nominal pixels of the tuner page (Panels.cpp, TunerView) shifted so that x = 0 is 450 left of the
page's centre and y = 0 is 50 below its top. The note and the needle are drawn by the app on top.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402
import amps  # noqa: E402
from common import CM  # noqa: E402

D = 2.4
UNIT = (20.0, 14.0, 860.0, 430.0)
NOTE = (150.0, 50.0, 600.0, 220.0)
NEEDLE = (60.0, 300.0, 780.0, 118.0)


def build(out, samples, scale):
    st = c.Stage((0.0, 0.0, 900.0, 470.0), D, scale=scale, samples=samples, floor=True)

    def rbox(name, r, z0, z1, mat, corner=0.0):
        p, q = st.at(r[0], r[1], z1), st.at(r[0] + r[2], r[1] + r[3], z1)
        return c.box(name, p.x, q.x, q.y, p.y, z0, z1, mat, corner=corner)

    alu = c.principled("tuner-alu", (0.034, 0.036, 0.04), metallic=0.92, rough=0.36)
    n = c.Nodes(alu)
    grain = n.noise(120.0, 4.0, stretch=(0.012, 22.0, 22.0))
    n.link(n.bump(grain, 0.5, 0.008), n.bsdf.inputs["Normal"])
    n.link(n.math("MULTIPLY_ADD", grain, 0.18, 0.28), n.bsdf.inputs["Roughness"])
    glass = c.principled("tuner-glass", (0.004, 0.0045, 0.005), rough=0.12, coat=1.0, coat_rough=0.02)
    body = rbox("unit", UNIT, 0.0, D, alu, corner=16 * CM)
    c.grid(body, 0.4)
    for name, r, rad in (("note", NOTE, 14), ("needle", NEEDLE, 12)):
        c.cut(body, rbox(name + "-well", r, D - 0.7, D + 1, c.black(), corner=rad * CM))
    mod = body.modifiers.new("chamfer", "BEVEL")
    mod.width, mod.segments, mod.limit_method, mod.angle_limit = 0.16, 3, "ANGLE", math.radians(50)
    body.modifiers.new("normals", "WEIGHTED_NORMAL")
    for name, r, rad in (("note", NOTE, 14), ("needle", NEEDLE, 12)):
        rbox(name + "-glass", r, D - 0.74, D - 0.7, glass, corner=rad * CM)
        # a chrome rim round each window, a hair proud of the panel
        rim = rbox(name + "-rim", (r[0] - 5, r[1] - 5, r[2] + 10, r[3] + 10), D - 0.02, D + 0.07, c.chrome(), corner=(rad + 5) * CM)
        c.cut(rim, rbox(name + "-rimcut", r, D - 1, D + 1, c.black(), corner=rad * CM))
        c.roll(rim, 0.03, 3)
    for sx in (UNIT[0] + 22, UNIT[0] + UNIT[2] - 22):
        for sy in (UNIT[1] + 22, UNIT[1] + UNIT[3] - 22):
            amps.screw_head(st, sx, sy, D, 0.2, c.chrome(), sx * 0.01 + sy * 0.02)
    for fx in (UNIT[0] + 70, UNIT[0] + UNIT[2] - 70 - 60):
        p, q = st.at(fx, UNIT[1] + UNIT[3] - 2, D * 0.5), st.at(fx + 60, UNIT[1] + UNIT[3] + 8, D * 0.5)
        ft = c.box("foot", p.x, q.x, q.y, p.y, D * 0.15, D * 0.85, c.rubber(), corner=0.15)
        c.roll(ft, 0.1, 3)
    st.render(os.path.join(out, "tuner_unit.png"))


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = args[0]
    samples, scale = 128, 2.0
    for i, a in enumerate(args):
        if a == "--samples": samples = int(args[i + 1])
        if a == "--scale": scale = float(args[i + 1])
    os.makedirs(out, exist_ok=True)
    build(out, samples, scale)


if __name__ == "__main__":
    main()
