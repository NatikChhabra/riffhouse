# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Photograph the cab page's microphone: a dynamic instrument mic held in a stand clip.

    blender -b -P mic.py -- <output folder> [--samples n] [--px n]

The cab is seen from the front, so here the "floor" of the shared stage is the speaker's baffle:
height is distance out from the cab toward the player. The app moves the mic across the cone and
toward the player, and the stand's foot always stands on the floor in front of the cab, so the
stand comes in three pictures the app puts together:

    mic-head   the mic, its clip and swivel, and the stand's rod running out of the bottom
    mic-rod    a short piece of that rod, stretched to whatever length is needed
    mic-base   the tripod: three legs, the hub and the height clutch (its top is 2.82 HR above the hub)

Every number is in nominal pixels for a mic whose head is HR = 30 across its radius: the app
scales the pictures by its own head size over HR. The points the app needs to line up are
printed at the end (and listed in the comment above drawCabMic in Gear.cpp).
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c  # noqa: E402
from common import CM  # noqa: E402
from mathutils import Vector  # noqa: E402

HR = 30.0
AXIS = (0.62, 0.785)          # head to tail, on screen: down and to the right
LEAN = math.radians(48.0)     # how far the mic's axis comes out of the screen toward us
HEAD_AREA = (-60.0, -60.0, 200.0, 200.0)
BASE_AREA = (-110.0, -100.0, 220.0, 160.0)
ROD_R = 0.17 * HR             # the rod's radius
Z_HEAD = 6.0                  # the grille's front, out from the cone


def mats():
    return {
        "body": c.principled("mic-body", (0.018, 0.018, 0.02), rough=0.38, coat=0.35, coat_rough=0.2),
        "grille": c.principled("mic-grille", (0.32, 0.32, 0.33), metallic=1.0, rough=0.36),
        "ring": c.principled("mic-ring", (0.7, 0.7, 0.71), metallic=1.0, rough=0.22),
        "plastic": c.principled("clip", (0.025, 0.025, 0.027), rough=0.42),
        "rod": c.principled("rod", (0.05, 0.05, 0.055), metallic=0.8, rough=0.3),
        "hole": c.principled("hole", (0.003, 0.003, 0.003), rough=0.8),
        "rubber": c.rubber(),
    }


def aim(ob, start, direction):
    """Point a lathe's +z along `direction` from `start`."""
    ob.rotation_mode = "QUATERNION"
    ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(direction.normalized())
    ob.location = start
    return ob


def mic_axis(st):
    """The head's front centre and the unit direction toward the tail, in the stage's space."""
    p0 = st.at(0.0, 0.0, Z_HEAD * CM)
    L = 100.0
    p1 = st.at(AXIS[0] * L * math.cos(LEAN), AXIS[1] * L * math.cos(LEAN), (Z_HEAD + L * math.sin(LEAN)) * CM)
    return p0, (p1 - p0).normalized()


def to_screen(st, p):
    """The nominal (x, y) where the stage's camera sees the 3D point p."""
    # at() is linear: invert it for this point's own height
    z = p.z
    x = p.x / CM + st.cx
    y = st.cy - (p.y * c.COS + (z - st.top) * c.SIN) / CM
    return x, y


def head(st, m):
    k = CM
    p0, d = mic_axis(st)
    obs = []
    # the grille: a flat front with a rolled rim, then slotted rings stepping back to the neck
    prof = [(0.0001, 0.0), (0.8 * HR * k, 0.0), (0.9 * HR * k, 0.08 * HR * k)]
    z = 0.12
    while z < 1.1:
        r = 0.9 - 0.08 * z
        prof += [(r * HR * k, z * HR * k), ((r - 0.06) * HR * k, (z + 0.04) * HR * k), ((r - 0.06) * HR * k, (z + 0.08) * HR * k), (r * HR * k, (z + 0.11) * HR * k)]
        z += 0.14
    prof += [(0.8 * HR * k, 1.18 * HR * k), (0.66 * HR * k, 1.26 * HR * k), (0.0001, 1.26 * HR * k)]
    obs.append(aim(c.lathe("grille", prof, m["grille"], seg=96), p0, d))
    # the dark mesh behind the front, seen through it
    obs.append(aim(c.lathe("front", [(0.0001, -0.01 * HR * k), (0.78 * HR * k, -0.01 * HR * k), (0.78 * HR * k, 0.0)], m["hole"], seg=64), p0, d))
    # a bright ring where the grille screws onto the body
    obs.append(aim(c.lathe("neck", [(0.0001, 1.24 * HR * k), (0.68 * HR * k, 1.24 * HR * k), (0.7 * HR * k, 1.3 * HR * k), (0.7 * HR * k, 1.38 * HR * k), (0.66 * HR * k, 1.42 * HR * k), (0.0001, 1.42 * HR * k)],
                           m["ring"], seg=96), p0, d))
    # the body: a long satin taper to the connector end
    obs.append(aim(c.lathe("body", [(0.0001, 1.4 * HR * k), (0.64 * HR * k, 1.4 * HR * k), (0.62 * HR * k, 2.0 * HR * k), (0.54 * HR * k, 3.7 * HR * k), (0.52 * HR * k, 3.82 * HR * k), (0.0001, 3.82 * HR * k)],
                           m["body"], seg=96), p0, d))
    # the cable's plug, pushed into the tail: a nickel shell, then the strain relief
    obs.append(aim(c.lathe("plug", [(0.0001, 3.8 * HR * k), (0.5 * HR * k, 3.8 * HR * k), (0.5 * HR * k, 4.25 * HR * k), (0.42 * HR * k, 4.35 * HR * k), (0.0001, 4.35 * HR * k)], m["ring"], seg=96), p0, d))
    for i in range(3):
        z0 = 4.34 + i * 0.12
        r = 0.36 - i * 0.05
        obs.append(aim(c.lathe("relief%d" % i, [(0.0001, z0 * HR * k), (r * HR * k, z0 * HR * k), (r * HR * k, (z0 + 0.1) * HR * k), ((r - 0.03) * HR * k, (z0 + 0.12) * HR * k), (0.0001, (z0 + 0.12) * HR * k)],
                               m["rubber"], seg=48), p0, d))
    tail = p0 + d * (4.7 * HR * k)
    # the clip: a ring round the body and a yoke down to the swivel on the rod
    t_clip = 2.35
    pc = p0 + d * (t_clip * HR * k)
    obs.append(aim(c.lathe("clip", [(0.6 * HR * k, -0.3 * HR * k), (0.74 * HR * k, -0.3 * HR * k), (0.76 * HR * k, -0.2 * HR * k), (0.76 * HR * k, 0.2 * HR * k), (0.74 * HR * k, 0.3 * HR * k),
                                    (0.6 * HR * k, 0.3 * HR * k), (0.6 * HR * k, -0.3 * HR * k)], m["plastic"], seg=96), pc, d))
    sx, sy = to_screen(st, pc)
    swivel_y = sy + 1.05 * HR
    sw = st.at(sx, swivel_y, pc.z)
    # the yoke: a flat strap from the ring's underside down to the swivel
    top = pc + (sw - pc).normalized() * (0.7 * HR * k)
    yoke = c.box("yoke", -0.22 * HR * k, 0.22 * HR * k, -0.1 * HR * k, 0.1 * HR * k, 0.0, (sw - top).length + 0.1 * HR * k, m["plastic"], edge=0.04 * HR * k)
    aim(yoke, top, sw - top)
    obs.append(yoke)
    # the swivel: a short barrel across, with a ribbed thumb nut on its end
    barrel = c.lathe("barrel", [(0.0001, -0.32 * HR * k), (0.3 * HR * k, -0.32 * HR * k), (0.3 * HR * k, 0.32 * HR * k), (0.0001, 0.32 * HR * k)], m["plastic"], seg=48)
    obs.append(aim(barrel, sw, Vector((1, 0, 0))))
    nut = c.lathe("nut", [(0.0001, 0.32 * HR * k), (0.42 * HR * k, 0.32 * HR * k), (0.42 * HR * k, 0.56 * HR * k), (0.0001, 0.56 * HR * k)], m["plastic"], seg=96, flutes=18, depth=0.1)
    obs.append(aim(nut, sw, Vector((1, 0, 0))))
    # the threaded adapter, and the rod going down out of the picture
    obs.append(aim(c.lathe("adapter", [(0.0001, 0.0), (0.26 * HR * k, 0.0), (0.26 * HR * k, 0.55 * HR * k), (0.0001, 0.55 * HR * k)], m["ring"], seg=48), sw + Vector((0, -0.2 * HR * k, 0)), Vector((0, -1, 0))))
    rod_top = sw + Vector((0, -0.7 * HR * k, 0))
    obs.append(aim(c.lathe("rod", [(0.0001, 0.0), (ROD_R * k, 0.0), (ROD_R * k, 400.0 * k), (0.0001, 400.0 * k)], m["rod"], seg=48), rod_top, Vector((0, -1, 0))))
    return obs, {"rod_x": sx, "rod_z": pc.z, "tail": to_screen(st, tail), "swivel": (sx, swivel_y)}


def base(st, m, rod_x, rod_z):
    """The tripod. The hub sits at nominal (rod_x, 0), right under the mic's rod so the light falls
    on both alike; the floor is 1.5 HR below it."""
    k = CM
    floor_y = 1.5 * HR
    hub = st.at(rod_x, 0.0, rod_z)
    obs = []
    obs.append(aim(c.lathe("hub", [(0.0001, -0.45 * HR * k), (0.42 * HR * k, -0.45 * HR * k), (0.46 * HR * k, -0.3 * HR * k), (0.46 * HR * k, 0.35 * HR * k), (0.3 * HR * k, 0.55 * HR * k), (0.0001, 0.55 * HR * k)],
                           m["plastic"], seg=64), hub, Vector((0, 1, 0))))
    # three legs: two out to the sides and a little back, one toward the player
    toes = []
    for ang in (90.0, 210.0, 330.0):
        a = math.radians(ang)
        toes.append(Vector((math.cos(a) * 2.9 * HR * k, 0.0, math.sin(a) * 2.9 * HR * k)))
    floor_world = st.at(rod_x, floor_y, rod_z).y
    for t in toes:
        toe = Vector((hub.x + t.x, floor_world + 0.18 * HR * k, rod_z + t.z))
        start = hub + Vector((0, -0.1 * HR * k, 0)) + (toe - hub).normalized() * (0.35 * HR * k)
        leg = c.lathe("leg", [(0.0001, 0.0), (0.16 * HR * k, 0.0), (0.16 * HR * k, (toe - start).length), (0.0001, (toe - start).length)], m["rod"], seg=32)
        obs.append(aim(leg, start, toe - start))
        foot = c.lathe("foot", [(0.0001, 0.0), (0.24 * HR * k, 0.0), (0.26 * HR * k, 0.12 * HR * k), (0.22 * HR * k, 0.3 * HR * k), (0.0001, 0.3 * HR * k)], m["rubber"], seg=32)
        obs.append(aim(foot, Vector((toe.x, floor_world, toe.z)), Vector((0, 1, 0))))
    # the rod up out of the hub, with its height clutch
    obs.append(aim(c.lathe("rodlow", [(0.0001, 0.0), (ROD_R * 1.15 * k, 0.0), (ROD_R * 1.15 * k, 2.4 * HR * k), (0.0001, 2.4 * HR * k)], m["rod"], seg=48), hub, Vector((0, 1, 0))))
    clutch = c.lathe("clutch", [(0.0001, 0.0), (0.36 * HR * k, 0.0), (0.4 * HR * k, 0.1 * HR * k), (0.4 * HR * k, 0.5 * HR * k), (0.3 * HR * k, 0.62 * HR * k), (0.0001, 0.62 * HR * k)],
                     m["plastic"], seg=96, flutes=16, depth=0.08)
    obs.append(aim(clutch, hub + Vector((0, 2.2 * HR * k, 0)), Vector((0, 1, 0))))
    # above the clutch the rod is the mic picture's own, so the two cannot be lit differently
    return obs


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    out = args[0]
    samples, px = 128, 400
    for i, a in enumerate(args):
        if a == "--samples": samples = int(args[i + 1])
        if a == "--px": px = int(args[i + 1])
    os.makedirs(out, exist_ok=True)

    st = c.Stage(HEAD_AREA, 0.0, scale=px / HEAD_AREA[2], samples=samples, floor=False)
    _, info = head(st, mats())
    st.render(os.path.join(out, "mic-head.png"))
    print("MIC", {k: (tuple(round(v, 2) for v in val) if isinstance(val, tuple) else round(val, 2)) for k, val in info.items()}, flush=True)

    rx = info["rod_x"]
    st = c.Stage((BASE_AREA[0] + rx, BASE_AREA[1], BASE_AREA[2], BASE_AREA[3]), 0.0, scale=px / BASE_AREA[2], samples=samples, floor=False)
    base(st, mats(), rx, info["rod_z"])
    st.render(os.path.join(out, "mic-base.png"))

    # the piece of rod is cut from the bottom of the mic's own picture, so it is lit the same
    from PIL import Image
    im = Image.open(os.path.join(out, "mic-head.png"))
    f = im.width / HEAD_AREA[2]
    x0 = (rx - 10.0 - HEAD_AREA[0]) * f
    im.crop((round(x0), round((120.0 - HEAD_AREA[1]) * f), round(x0 + 20.0 * f), round((140.0 - HEAD_AREA[1]) * f))).save(os.path.join(out, "mic-rod.png"))


if __name__ == "__main__":
    main()
