// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"

namespace rh
{
// Passive treble/mid/bass tone stack, discretised from its analog transfer
// function with the bilinear transform (after D. T. Yeh and J. O. Smith,
// "Discretization of the '59 Fender Bassman Tone Stack", DAFx 2006).
// Component values decide the voicing; different values give the
// American, British and other classic tone stack characters.
struct ToneStackComponents
{
    double R1, R2, R3, R4; // treble pot, bass pot, mid pot, slope resistor (ohms)
    double C1, C2, C3;     // treble, bass and mid caps (farads)
};

class ToneStack
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept { x1 = x2 = x3 = y1 = y2 = y3 = 0.0; }

    // t, m, l in 0..1. The bass and mid pots are log taper on most amps.
    void set (const ToneStackComponents& c, double t, double m, double l) noexcept
    {
        t = std::clamp (t, 0.0, 1.0);
        m = std::clamp (m, 0.0, 1.0);
        l = std::clamp (l, 0.0, 1.0);
        // audio taper for bass and mid pots
        l = (std::pow (10.0, 2.0 * l) - 1.0) / 99.0;
        m = (std::pow (10.0, 2.0 * m) - 1.0) / 99.0;
        // keep away from exact zero so the filter never degenerates
        t = 0.002 + 0.996 * t;
        m = 0.002 + 0.996 * m;
        l = 0.002 + 0.996 * l;

        const double R1 = c.R1, R2 = c.R2, R3 = c.R3, R4 = c.R4, C1 = c.C1, C2 = c.C2, C3 = c.C3;

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
                          - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                          + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                          + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                          + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                          + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                          - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                          + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                          + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                          + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                          + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                          - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                          + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                          + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                          - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                          + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                          + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        const double k = 2.0 * fs, k2 = k * k, k3 = k2 * k;

        const double B0 = -b1 * k - b2 * k2 - b3 * k3;
        const double B1 = -b1 * k + b2 * k2 + 3.0 * b3 * k3;
        const double B2 = b1 * k + b2 * k2 - 3.0 * b3 * k3;
        const double B3 = b1 * k - b2 * k2 + b3 * k3;
        const double A0 = -1.0 - a1 * k - a2 * k2 - a3 * k3;
        const double A1 = -3.0 - a1 * k + a2 * k2 + 3.0 * a3 * k3;
        const double A2 = -3.0 + a1 * k + a2 * k2 - 3.0 * a3 * k3;
        const double A3 = -1.0 + a1 * k - a2 * k2 + a3 * k3;

        nb0 = B0 / A0; nb1 = B1 / A0; nb2 = B2 / A0; nb3 = B3 / A0;
        na1 = A1 / A0; na2 = A2 / A0; na3 = A3 / A0;
    }

    float process (float xf) noexcept
    {
        const double x = xf;
        const double y = nb0 * x + nb1 * x1 + nb2 * x2 + nb3 * x3 - na1 * y1 - na2 * y2 - na3 * y3;
        x3 = x2; x2 = x1; x1 = x;
        y3 = y2; y2 = y1; y1 = std::abs (y) < 1e-25 ? 0.0 : y;
        return (float) y;
    }

    double magnitude (double hz) const noexcept
    {
        const double w = kTwoPi * hz / fs;
        auto cpx = [w] (double a0, double a1, double a2, double a3, double& re, double& im)
        {
            re = a0 + a1 * std::cos (w) + a2 * std::cos (2 * w) + a3 * std::cos (3 * w);
            im = -(a1 * std::sin (w) + a2 * std::sin (2 * w) + a3 * std::sin (3 * w));
        };
        double nr, ni, dr, di;
        cpx (nb0, nb1, nb2, nb3, nr, ni);
        cpx (1.0, na1, na2, na3, dr, di);
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    double fs = 48000.0;
    double nb0 = 0, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0;
    double x1 = 0, x2 = 0, x3 = 0, y1 = 0, y2 = 0, y3 = 0;
};

namespace tonestacks
{
    // American: big bass pot, small mid pot, scooped mids.
    constexpr ToneStackComponents american { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 };
    // Narrow-panel tweed style.
    constexpr ToneStackComponents tweed { 250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9 };
    // British: smaller slope resistor and caps, more upper mids.
    constexpr ToneStackComponents british { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 };
    // Hot-rodded British with a bigger mid range.
    constexpr ToneStackComponents britishHot { 220e3, 1e6, 25e3, 39e3, 500e-12, 22e-9, 22e-9 };
    // Modern high gain: deep bass, tight scooped low mids.
    constexpr ToneStackComponents modern { 250e3, 1e6, 25e3, 47e3, 500e-12, 22e-9, 22e-9 };
    // Chimey top-boost style.
    constexpr ToneStackComponents chime { 1e6, 1e6, 10e3, 100e3, 50e-12, 22e-9, 22e-9 };
} // namespace tonestacks
} // namespace rh
