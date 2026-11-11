# Riffhouse - free, open-source guitar rig
# Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
"""Build and photograph every pedal.

    python pedals.py <gear folder written by `Riffhouse --snapshots`> <output folder> [--only id,id] [--samples n] [--scale s]

The app says where each part sits (gear.json) and hands over the print for the face. This script
builds the box around that: six kinds of enclosure, six finishes, the sockets, the light's bezel
and the switch. Knobs are photographed on their own (parts.py) because they turn.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import common as c
from common import CM

HEIGHT = {0: 3.1, 1: 3.4, 2: 3.3, 3: 2.3, 4: 2.9, 5: 2.7}   # top face above the floor, cm


def slug(name):
    """'GREEN DRIVE' becomes 'green_drive': the same spelling the app looks its pictures up by."""
    return "".join(ch if ch.isalnum() else "_" for ch in name.lower())


def prepare_print(src, dst_colour, dst_mask, ink_hex):
    """The app's print as two pictures: solid ink colour everywhere, and how much ink there is."""
    im = np.asarray(Image.open(src).convert("RGBA")).copy()
    ink = [int(ink_hex[i:i + 2], 16) for i in (0, 2, 4)]
    clear = im[:, :, 3] == 0
    for k in range(3):
        im[:, :, k][clear] = ink[k]
    Image.fromarray(im[:, :, :3], "RGB").save(dst_colour)
    Image.fromarray(im[:, :, 3], "L").save(dst_mask)


def printed(material, colour_path, mask_path):
    n = c.Nodes(material)
    col = n.image(colour_path)
    mask = n.image(mask_path, colorspace="Non-Color")
    a = mask.outputs["Color"]
    b = n.bsdf

    def take(name):
        s = b.inputs[name]
        return s.links[0].from_socket if s.links else s.default_value

    base = take("Base Color")
    n.link(n.mix(a, base if not hasattr(base, "__len__") else tuple(base), col.outputs["Color"]), b.inputs["Base Color"])
    for name, ink_value in (("Metallic", 0.0), ("Roughness", 0.5), ("Coat Weight", 0.0)):
        cur = take(name)
        inv = n.math("SUBTRACT", 1.0, a)
        n.link(n.math("ADD", n.math("MULTIPLY", inv, cur), n.math("MULTIPLY", a, ink_value)), b.inputs[name])
    return material


# ------------------------------------------------------------------ parts that sit on a face
def led_bezel(st, bodies, x, y, lens_r, led_hex, parts):
    p, nrm = st.on(bodies, x, y)
    r = lens_r
    bz = c.lathe("bezel", [(r * 1.75, 0), (r * 1.75, r * 0.3), (r * 1.55, r * 0.75), (r * 1.3, r * 0.95), (r * 1.08, r * 0.95), (r * 1.03, r * 0.4)],
                 parts["chrome"], seg=64)
    col = c.rgb(led_hex)
    lens = c.principled("lens", tuple(v * 0.16 + 0.004 for v in col), rough=0.1, coat=1.0, coat_rough=0.03)
    ln = c.lathe("lens", [(r * 1.03, r * 0.2), (r * 1.03, r * 0.75), (r * 0.85, r * 1.2), (r * 0.5, r * 1.48), (0.0001, r * 1.56)], lens, seg=48)
    for ob in (bz, ln):
        c.stand(ob, p, nrm)


def stomp_switch(st, bodies, x, y, fs, parts):
    """A stomp switch: washer, hex nut, threaded bush and the polished plunger."""
    p, nrm = st.on(bodies, x, y)
    k = fs / 1.72
    obs = [
        c.lathe("washer", [(0.84 * k, 0), (0.84 * k, 0.04 * k), (0.82 * k, 0.055 * k)], parts["steel"], seg=64),
        c.prism("hexnut", 6, 0.74 * k, 0.05 * k, 0.28 * k, parts["nickel"], round_edges=0.035 * k),
        c.lathe("bush", [(0.5 * k, 0.28 * k), (0.5 * k, 0.6 * k), (0.47 * k, 0.64 * k), (0.4 * k, 0.64 * k)], parts["chrome"],
                seg=176, flutes=44, depth=0.04, flute_rings=(0, 1)),
        c.lathe("plunger", [(0.36 * k, 0.5 * k), (0.36 * k, 0.9 * k), (0.34 * k, 0.97 * k), (0.28 * k, 1.02 * k), (0.15 * k, 1.05 * k), (0.0001, 1.06 * k)],
                parts["chrome"], seg=64),
    ]
    for ob in obs:
        c.stand(ob, p, nrm, turn=math.radians(30))


def soft_switch(st, bodies, x, y, fs, parts):
    """A low soft-touch switch in a counterbore: a satin ring round a dished black button."""
    p, nrm = st.on(bodies, x, y)
    r = fs * 0.5
    ring = c.lathe("ring", [(r * 0.98, 0), (r * 0.98, 0.1), (r * 0.94, 0.16), (r * 0.8, 0.17), (r * 0.78, 0.08)], parts["chrome"], seg=96)
    cap = c.lathe("cap", [(r * 0.78, 0.05), (r * 0.78, 0.2), (r * 0.74, 0.25), (r * 0.6, 0.24), (r * 0.3, 0.215), (0.0001, 0.205)],
                  c.principled("softcap", (0.02, 0.021, 0.023), rough=0.42), seg=96)
    for ob in (ring, cap):
        c.stand(ob, p, nrm)


def side_jacks(st, x0, x1, y_nominal, z, parts, s=1.0):
    """The two sockets in the side walls."""
    for side, x in ((-1, x0), (1, x1)):
        pos = st.at(st.cx, y_nominal, z)
        for name, r, l0, l1, sides, m in (("jnut", 0.52, 0.0, 0.2, 6, "nickel"), ("jbody", 0.37, 0.2, 0.46, 48, "steel"), ("jtip", 0.3, 0.46, 0.5, 48, "black")):
            ob = c.prism(name, sides, r * s, l0 * s, l1 * s, parts[m], round_edges=0.02, smooth=sides > 6)
            ob.rotation_euler = (0, side * math.pi / 2, 0)
            ob.location = (x - side * 0.02, pos.y, z)


def tread_plate(width, length, paint, parts, u, thickness=0.5):
    """A plate with a ribbed rubber mat on it, lying level, built round its own middle with its top at z = 0."""
    holder = c.bpy.data.objects.new("plate", None)
    c.bpy.context.scene.collection.objects.link(holder)
    made = [holder]
    def child(ob):
        ob.parent = holder
        made.append(ob)
        return ob
    hw, hl = width / 2, length / 2
    c.roll(child(c.box("plate", -hw, hw, -hl, hl, -thickness, 0.0, paint, corner=4 * u)), 0.06, 4)
    mx0, mx1 = -hw + 9 * u, hw - 9 * u
    my1, my0 = hl - 15 * u, -hl + 10 * u
    c.roll(child(c.box("mat", mx0, mx1, my0, my1, -0.02, 0.05, parts["rubber"], corner=5 * u)), 0.02, 3)
    pitch = 9.5 * u
    ribs = max(3, int(((my1 - my0) - 10 * u) / pitch))
    y0 = (my0 + my1) / 2 + ribs * pitch / 2 - pitch * 0.25
    for i in range(ribs):
        yy = y0 - i * pitch
        rib = child(c.box("rib", mx0 + 8 * u, mx1 - 8 * u, yy - pitch * 0.5, yy, 0.03, 0.12, parts["rubber"], corner=pitch * 0.24, corner_segments=5))
        c.roll(rib, 0.03, 3)
    return holder, made, child


def foot_plate(st, rect, top, paint, parts, u):
    """The hinged plate of a box whose whole lower half is the switch: paint, a hinge pin and a ribbed rubber mat."""
    x, y, w, h = rect
    a, b = st.at(x, y, top), st.at(x + w, y + h, top)
    length = a.y - b.y
    plate, _, child = tread_plate(b.x - a.x, length, paint, parts, u)
    pin = c.prism("pin", 24, 0.11, 0, (b.x - a.x) - 16 * u, parts["nickel"], smooth=True)
    pin.rotation_euler = (0, math.pi / 2, 0)
    pin.location = (-(b.x - a.x) / 2 + 8 * u, length / 2 - 6 * u, 0.03)
    child(pin)
    hinge = c.bpy.data.objects.new("hinge", None)                   # the hinge line is the far end
    c.bpy.context.scene.collection.objects.link(hinge)
    hinge.location = ((a.x + b.x) / 2, a.y, top + 0.02)
    plate.parent = hinge
    plate.location = (0, -length / 2, 0)
    hinge.rotation_euler = (-math.atan2(0.3, length), 0, 0)         # the near end stands higher: that is the end you press
    return hinge


TREADLE_FRAMES = 11
TREADLE_SWING = math.radians(8.0)


def treadle_area(rect, un):
    """The patch of the picture a rocking treadle can reach (the app cuts the same patch: treadleArtArea)."""
    x, y, w, h = rect
    return (x - 16 * un, y - 30 * un, w + 32 * un, h + 40 * un)


def treadle(st, rect, top, paint, parts, u, position):
    """The rocking plate of a wah, on its pivot above the trough. position 0 is heel down, 1 is toe down."""
    x, y, w, h = rect
    a, b = st.at(x, y, top), st.at(x + w, y + h, top)
    plate, made, child = tread_plate(b.x - a.x, a.y - b.y, paint, parts, u, thickness=0.45)
    axle = c.prism("axle", 20, 0.16, 0, (b.x - a.x) + 22 * u, parts["steel"], smooth=True)
    axle.rotation_euler = (0, math.pi / 2, 0)
    axle.location = (-(b.x - a.x) / 2 - 11 * u, 0, -0.45)
    child(axle)
    plate.location = ((a.x + b.x) / 2, (a.y + b.y) / 2, top + 0.85)
    plate.rotation_euler = ((0.5 - position) * 2.0 * TREADLE_SWING, 0, 0)
    return made


def tread_strip(st, body, box_rect, folder, pid, parts):
    """Chequer plate along the near edge: a steel strip with raised lozenges."""
    x, y, w, h = box_rect
    area = st.area
    scale = st.scene.render.resolution_x / area[2]
    im = Image.new("L", (st.scene.render.resolution_x, st.scene.render.resolution_y), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(im)
    sy0, sy1 = y + h * 0.91, y + h
    step = h / 420.0
    yy = sy0 + 4 * step
    row = 0
    while yy < sy1 - 2 * step:
        xx = x - 20 * step + (7 * step if row % 2 else 0)
        while xx < x + w + 10 * step:
            ang = 0.8 if row % 2 == 0 else -0.8
            cx, cy = (xx - area[0]) * scale, (yy - area[1]) * scale
            L, T = 5.2 * step * scale, 1.5 * step * scale
            pts = []
            for t in range(24):
                a = 2 * math.pi * t / 24
                px, py = L * math.cos(a), T * math.sin(a)
                pts.append((cx + px * math.cos(ang) - py * math.sin(ang), cy + px * math.sin(ang) + py * math.cos(ang)))
            d.polygon(pts, fill=255)
            xx += 14 * step
        yy += 10 * step
        row += 1
    from PIL import ImageFilter
    im = im.filter(ImageFilter.GaussianBlur(1.6))
    path = os.path.join(folder, pid + "-tread.png")
    im.save(path)
    m = c.principled("tread", (0.46, 0.47, 0.48), metallic=1.0, rough=0.4)
    n = c.Nodes(m)
    tex = n.image(path, colorspace="Non-Color")
    n.link(n.bump(tex.outputs["Color"], 0.8, 0.022), n.bsdf.inputs["Normal"])
    # the strip follows the face: four corners found by looking through the camera
    import bmesh
    bm = bmesh.new()
    inset = 0.4 * step
    corners = []
    # found on the far edge of the strip, where the face is sure to be flat, and carried down the face from there
    _, nrm = st.on([body], x + w / 2, sy0)

    def edge(from_x, step_x):                # walk in from the side until the ray lands on the flat of the face
        xx = from_x
        for _ in range(60):
            hit, n = st.on([body], xx, sy0)
            if n.dot(nrm) > 0.995:
                return hit
            xx += step_x
        return hit
    pa, pb = edge(x + inset, step), edge(x + w - inset, -step)
    down = c.Vector((0.0, -1.0, 0.0))
    down = (down - nrm * down.dot(nrm)).normalized()          # toward the player, along the face
    run = (sy1 - inset - sy0) * CM / c.COS / max(0.2, abs(down.y))
    corners = [(pa, nrm), (pb, nrm), (pb + down * run, nrm), (pa + down * run, nrm)]
    lower = [bm.verts.new(p) for p, _ in corners]
    upper = [bm.verts.new(p + nrm * 0.07) for p, nrm in corners]
    bm.faces.new(upper)
    for i in range(4):
        bm.faces.new((lower[i], lower[(i + 1) % 4], upper[(i + 1) % 4], upper[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = c.bpy.data.meshes.new("tread")
    bm.to_mesh(me)
    bm.free()
    c.link(me, "tread", m, smooth=False)


# ------------------------------------------------------------------ one pedal
def build(p, gear_dir, out_dir, samples, scale):
    pid, form = p["id"], p["form"]
    H = HEIGHT[form]
    st = c.Stage(p["area"], H, scale=scale, samples=samples)
    bx, by, bw, bh = p["box"]
    u = bh / 420.0 * CM                       # the app's own unit, in cm
    un = bh / 420.0                           # ... and in nominal pixels
    tl, br = st.at(bx, by), st.at(bx + bw, by + bh)
    x0, x1, y1, y0 = tl.x, br.x, tl.y, br.y   # y1 is the far end

    colour, mask = os.path.join(out_dir, "_work", pid + "-ink.png"), os.path.join(out_dir, "_work", pid + "-mask.png")
    os.makedirs(os.path.dirname(colour), exist_ok=True)
    # a knocked-out name on a band is printed in the box's own colour: transparent pixels take the band's ink
    prepare_print(os.path.join(gear_dir, pid + "-print.png"), colour, mask, p["ink"])
    paint = printed(c.finish_material(p["finish"], p["body"]), colour, mask)
    parts = {"chrome": c.chrome(), "nickel": c.nickel(), "steel": c.steel(), "black": c.black(), "rubber": c.rubber()}
    lx, ly, lw, lh = p["ledBox"]
    fx, fy, fw, fh = p["footswitch"]
    fs = fw * CM
    bodies = []

    if form in (0, 1, 5):
        rad = bw * 0.045 * CM
        body = c.box("body", x0, x1, y0, y1, 0.16, H, paint, corner=rad, taper=(0.978, 0.986))
        bodies.append(body)
        if form == 1:
            nx, ny, nw, nh = p["nameBox"]
            deck = (bx + bw * 0.055, by + bh * 0.03, bw * 0.89, ny - by - bh * 0.05)
            a, b = st.at(deck[0], deck[1]), st.at(deck[0] + deck[2], deck[1] + deck[3])
            c.cut(body, c.box("deck", a.x, b.x, b.y, a.y, H - 0.2, H + 1, paint, corner=7 * u))
            well = (fx - 3.5 * un, fy - 3.5 * un, fw + 7 * un, fh + 7 * un)
            a, b = st.at(well[0], well[1]), st.at(well[0] + well[2], well[1] + well[3])
            c.cut(body, c.box("well", a.x, b.x, b.y, a.y, H - 0.75, H + 1, parts["black"], corner=6 * u))
            foot_plate(st, p["footswitch"], H, paint, parts, u)
        if form == 5:
            tx, ty, tw, th = p["treadle"]
            well = (tx - 5 * un, ty - 5 * un, tw + 10 * un, th + 10 * un)
            a, b = st.at(well[0], well[1]), st.at(well[0] + well[2], well[1] + well[3])
            c.cut(body, c.box("trough", a.x, b.x, b.y, a.y, H - 1.0, H + 1, parts["black"], corner=8 * u))
            for hx in (tx - 15 * un, tx + tw + 15 * un):
                c.prism("pivot", 6, 6.5 * u, 0, 0.22, parts["nickel"], at=st.at(hx, ty + th / 2), rot=math.radians(30), round_edges=0.03)
        c.roll(body, 0.2 if form != 1 else 0.16, 8, 50)
        c.roll(c.box("plate", x0, x1, y0, y1, 0.0, 0.15, parts["steel"], corner=rad), 0.03, 2)
    elif form == 2:
        # folded sheet steel: a level top for the controls, then the face slopes down toward the player
        ny = p["nameBox"][1]
        fold = st.at(st.cx, ny - bh * 0.022).y
        low = H - 1.25
        near = st.at(st.cx, by + bh, low).y
        body = c.extrude_profile("body", [(y1, 0.2), (y1, H), (fold, H), (near, low), (near, 0.2)], x0, x1, paint)
        bodies.append(body)
        c.roll(body, 0.07, 3, 10)
        e = 1.5 * u
        c.roll(c.box("base", x0 - e, x1 + e, near - e, y1 + e, 0.0, 0.2, parts["steel"], corner=0.1), 0.03, 2)
    elif form == 3:
        # machined from a block: a flat chamfer all round, a counterbore for the switch
        body = c.box("body", x0, x1, y0, y1, 0.0, H, paint, corner=bw * 0.028 * CM)
        bodies.append(body)
        bore = fs * 0.5 + 3 * u
        cyl = c.prism("bore", 64, bore, H - 0.14, H + 1, parts["black"], at=st.at(fx + fw / 2, fy + fh / 2) - c.Vector((0, 0, H)), smooth=True)
        c.cut(body, cyl)
        mod = body.modifiers.new("chamfer", "BEVEL")
        mod.width, mod.segments, mod.limit_method, mod.angle_limit = 0.17, 1, "ANGLE", math.radians(60)
        mod2 = body.modifiers.new("ease", "BEVEL")
        mod2.width, mod2.segments, mod2.limit_method, mod2.angle_limit = 0.012, 2, "ANGLE", math.radians(30)
        for poly in body.data.polygons:
            poly.use_smooth = False
    elif form == 4:
        # a steel panel between two hardwood end cheeks that stand proud of it
        cheek = bw * 0.095 * CM
        body = c.box("body", x0 + cheek * 0.8, x1 - cheek * 0.8, y0, y1, 0.0, H, paint)
        c.roll(body, 0.04, 3)
        bodies.append(body)
        for i, (a, b) in enumerate(((x0, x0 + cheek), (x1 - cheek, x1))):
            ck = c.box("cheek", a, b, y0 - 4 * u, y1 + 4 * u, 0.0, H + 0.5, c.wood(seed=3.1 * i + int(p["body"], 16) % 7), corner=0.14, corner_segments=5)
            c.roll(ck, 0.2, 6)
        # the panel is held to the cheeks by four black screws
        for sx in (bx + bw * 0.095 * 0.8 + 15 * un, bx + bw - bw * 0.095 * 0.8 - 15 * un):
            for sy in (by + 15 * un, by + bh - 15 * un):
                at = st.at(sx, sy, H)
                c.lathe("screw", [(0.2, -0.02), (0.2, 0.035), (0.17, 0.06), (0.0001, 0.065)], parts["black"], seg=32).location = at
                slot = c.box("slot", -0.15, 0.15, -0.022, 0.022, 0.05, 0.08, parts["rubber"])
                slot.location, slot.rotation_euler[2] = at, 0.6 + sx * 0.37 + sy * 0.11

    # the light, the switch, the sockets
    led_bezel(st, bodies, lx + lw / 2, ly + lh / 2, (lw / 2) * CM, p["led"], parts)
    if form == 5:
        pass                                   # the treadle is photographed on its own: it rocks
    elif p["footStyle"] == 0:
        stomp_switch(st, bodies, fx + fw / 2, fy + fh / 2, fs, parts)
    elif p["footStyle"] == 2:
        soft_switch(st, bodies, fx + fw / 2, fy + fh / 2, fs, parts)
    if form != 4:
        jy = by + bh * (0.13 if form == 5 else (0.19 if form == 1 else 0.22))
        side_jacks(st, x0, x1, jy, H * 0.52, parts)
    if p["stripe"] == 2:
        tread_strip(st, bodies[0], p["box"], os.path.join(out_dir, "_work"), pid, parts)

    out = os.path.join(out_dir, "pedal_" + slug(p["name"]) + ".png")
    st.render(out)
    if form == 5:
        # the treadle in every position it can stand in: only the patch it can reach is photographed again
        tx, ty, tw, th = treadle_area(p["treadle"], un)
        ax, ay, aw, ah = p["area"]
        r = st.scene.render
        r.use_border, r.use_crop_to_border = True, True
        r.border_min_x, r.border_max_x = (tx - ax) / aw, (tx + tw - ax) / aw
        r.border_min_y, r.border_max_y = 1.0 - (ty + th - ay) / ah, 1.0 - (ty - ay) / ah
        for f in range(TREADLE_FRAMES):
            made = treadle(st, p["treadle"], H, paint, parts, u, f / (TREADLE_FRAMES - 1))
            st.render(os.path.join(out_dir, "_strips", "pedal_" + slug(p["name"]) + "_treadle", "%02d.png" % f))
            for ob in made:
                c.bpy.data.objects.remove(ob, do_unlink=True)
    return out


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    gear_dir, out_dir = args[0], args[1]
    only = None
    samples, scale = 128, 2.0
    for i, a in enumerate(args):
        if a == "--only": only = set(args[i + 1].split(","))
        if a == "--samples": samples = int(args[i + 1])
        if a == "--scale": scale = float(args[i + 1])
    os.makedirs(out_dir, exist_ok=True)
    gear = json.load(open(os.path.join(gear_dir, "gear.json")))
    for p in gear["pedals"]:
        if only and p["id"] not in only:
            continue
        print("pedal", p["id"], flush=True)
        build(p, gear_dir, out_dir, samples, scale)


if __name__ == "__main__":
    main()
