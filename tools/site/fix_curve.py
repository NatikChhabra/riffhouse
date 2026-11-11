"""Draw the two lines of the "No interface needed" figure on the website.

Nothing here is made up: the lines come from the filter the app itself runs (InputConditioner
in src/dsp/Pedals.cpp). The loss of a pickup into a mic jack is the app's own model of it, a
first-order fall that levels off at 4.5 kHz; Input Fix is that curve upside down followed by
the gentle top roll-off the app adds once the lift is large. The second line is the two added.

    python3 tools/site/fix_curve.py 21      # prints the two SVG paths for a 21 dB fix
"""
import cmath
import math
import sys

SR = 48000.0
TOP_HZ = 4500.0
W, H = 1200.0, 360.0
F_LO, F_HI = 80.0, 12000.0
DB_HI, DB_LO = 6.0, -30.0


def shelf(db):
    low, high = TOP_HZ / 10 ** (db / 20), TOP_HZ
    k1 = 1 / math.tan(math.pi * low / SR)
    k2 = 1 / math.tan(math.pi * min(high, 0.45 * SR) / SR)
    return ((1 + k1) / (1 + k2), (1 - k1) / (1 + k2), 0.0), (1.0, (1 - k2) / (1 + k2), 0.0)


def lowpass(hz, q):
    w = 2 * math.pi * hz / SR
    a = math.sin(w) / (2 * q)
    c = math.cos(w)
    a0 = 1 + a
    return ((1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0), (1.0, -2 * c / a0, (1 - a) / a0)


def gain_db(filt, hz):
    b, a = filt
    z = cmath.exp(-1j * 2 * math.pi * hz / SR)
    h = (b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z)
    return 20 * math.log10(abs(h))


def x_of(hz):
    return W * math.log(hz / F_LO) / math.log(F_HI / F_LO)


def y_of(db):
    return H * (DB_HI - db) / (DB_HI - DB_LO)


def path(fn):
    pts = []
    for i in range(121):
        hz = F_LO * (F_HI / F_LO) ** (i / 120)
        pts.append(f"{x_of(hz):.0f} {y_of(fn(hz)):.1f}")
    return "M" + " L".join(pts)


def main(db=21.0):
    lift = shelf(db)
    top_hz = 12000.0 + (5200.0 - 12000.0) * max(0.0, min(1.0, (db - 3.0) / 9.0))
    top = lowpass(top_hz, 0.85)
    floor = gain_db(lift, 20.0)                      # the shelf's level at the bottom, taken as 0 dB
    jack = lambda hz: -(gain_db(lift, hz) - floor)
    fixed = lambda hz: jack(hz) + (gain_db(lift, hz) - floor) + gain_db(top, hz)
    print("jack ", path(jack))
    print("fixed", path(fixed))
    print("at 3 kHz: jack %.1f dB, fixed %.1f dB" % (jack(3000), fixed(3000)))
    for hz in (100, 1000, 10000):
        print(f"{hz} Hz at {100 * x_of(hz) / W:.1f}%")
    for d in (0, -10, -20):
        print(f"{d} dB at y={y_of(d):.0f} ({100 * y_of(d) / H:.1f}%)")


if __name__ == "__main__":
    main(float(sys.argv[1]) if len(sys.argv) > 1 else 21.0)
