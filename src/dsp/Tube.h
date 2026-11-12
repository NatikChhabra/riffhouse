// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"

#include <array>
#include <vector>

// A preamp triode stage, solved as the circuit it is. The tube is the 12AX7 as Norman Koren's
// equations describe it (plate current from plate and grid voltage, fitted to the data sheet
// curves), standing on its load line: supply, plate resistor, cathode resistor and its bypass
// capacitor. In front of it sit the coupling capacitor, the grid-leak resistor and the grid
// stopper. Driven hard, the grid pulls current when it goes above the cathode; that charges
// the coupling capacitor and shifts the bias for a moment after: the "blocking" that makes a
// hard-pushed amp breathe and sag on chords. The stopper and the tube's own input capacitance
// (Miller effect) take the fizz off the top, more so in high-gain stages.
//
// The table holds the tube on its load line, solved once per stage; each sample is a lookup,
// a capacitor step and one Newton correction for the cathode. Volts in, volts out (signal on the
// plate, its resting voltage taken off).
namespace rh::tube
{
struct Triode
{
    double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0; // 12AX7
    double cgk = 1.6e-12, cgp = 1.7e-12;

    // plate current in amps
    double current (double vp, double vgk) const noexcept
    {
        if (vp <= 0.0) return 0.0;
        const double a = kp * (1.0 / mu + vgk / std::sqrt (kvb + vp * vp));
        const double soft = a > 30.0 ? a : std::log1p (std::exp (a));
        const double e1 = vp / kp * soft;
        return e1 > 0.0 ? std::pow (e1, ex) / kg1 : 0.0;
    }
};

// The plate voltage for each grid-to-cathode voltage, with the plate on its load line.
class LoadLine
{
public:
    static constexpr double kLo = -16.0, kHi = 2.0;
    static constexpr int kSize = 4096;

    void build (const Triode& t, double supply, double rPlate)
    {
        b = supply; rp = rPlate;
        table.resize ((size_t) kSize + 1);
        for (int i = 0; i <= kSize; ++i)
        {
            const double vgk = kLo + (kHi - kLo) * i / kSize;
            // b - vp = ip(vp) rp: the left side falls with vp, the right rises; bisect
            double lo = 0.0, hi = b;
            for (int it = 0; it < 60; ++it)
            {
                const double mid = 0.5 * (lo + hi);
                if (b - mid > t.current (mid, vgk) * rp) lo = mid; else hi = mid;
            }
            table[(size_t) i] = 0.5 * (lo + hi);
        }
        scale = kSize / (kHi - kLo);
    }

    // plate voltage and its slope against the grid voltage
    double plate (double vgk, double& slope) const noexcept
    {
        const double x = (std::clamp (vgk, kLo, kHi - 1.0e-9) - kLo) * scale;
        const int i = (int) x;
        const double f = x - i, y0 = table[(size_t) i], y1 = table[(size_t) i + 1];
        slope = (y1 - y0) * scale;
        return y0 + f * (y1 - y0);
    }
    double supply() const noexcept { return b; }
    double plateResistor() const noexcept { return rp; }

private:
    std::vector<double> table;
    double b = 300.0, rp = 100000.0, scale = 1.0;
};

// The usual preamp load line: 300 V through 100 kOhm. Built once (it takes a moment), then shared.
inline const LoadLine& standardLine()
{
    static const LoadLine line = [] { LoadLine l; l.build (Triode {}, 300.0, 100000.0); return l; }();
    return line;
}

struct StageParts
{
    double rCathode = 1500.0, cCathode = 22e-6;
    double cCoupling = 22e-9, rGridLeak = 1.0e6, rStopper = 68000.0;
};

class Stage
{
public:
    // Cheap enough for the audio thread once standardLine() has been built.
    // Asked again with the same parts (a knob turned elsewhere), it keeps playing undisturbed.
    void prepare (double fs, const StageParts& p, const LoadLine& l = standardLine())
    {
        auto same = [] (double a, double b) { return ! (a < b) && ! (b < a); };
        if (line == &l && same (dt, 1.0 / fs) && same (p.rCathode, parts.rCathode) && same (p.cCathode, parts.cCathode)
            && same (p.cCoupling, parts.cCoupling) && same (p.rGridLeak, parts.rGridLeak) && same (p.rStopper, parts.rStopper))
            return;
        parts = p; line = &l; dt = 1.0 / fs;
        supply = l.supply(); rPlate = l.plateResistor();
        // resting point: the cathode sits where its resistor carries the plate current
        // (vk - ip(-vk) rk rises with vk: bisect)
        double lo = 0.0, hi = 15.0;
        for (int it = 0; it < 80; ++it)
        {
            const double vkTry = 0.5 * (lo + hi);
            double s;
            const double ip = (supply - line->plate (-vkTry, s)) / rPlate;
            if (vkTry - ip * p.rCathode < 0.0) lo = vkTry; else hi = vkTry;
        }
        restVk = 0.5 * (lo + hi);
        double s;
        restVp = line->plate (-restVk, s);
        gainAtRest = -s;
        // a cathode left unbypassed (or nearly) feeds back: the gain falls by 1 + gm Rk
        const double gm0 = -s / rPlate;
        if (p.cCathode * p.rCathode < 1.0 / (kTwoPi * 300.0)) gainAtRest /= 1.0 + gm0 * p.rCathode;
        // input capacitance seen through the stopper: the grid-cathode capacitance plus the
        // plate capacitance times the stage's gain (Miller)
        const double cin = tube.cgk + (1.0 + gainAtRest) * tube.cgp;
        const double hz = 1.0 / (kTwoPi * std::max (1.0, p.rStopper) * cin);
        const double g = std::tan (kPi * std::min (hz, 0.45 / dt) * dt);
        millerG = g / (1.0 + g);
        reset();
    }

    void reset() noexcept { vc = 0.0; vk = restVk; miller = 0.0; lastA = 0.0; }

    double restPlate() const noexcept { return restVp; }
    double restCathode() const noexcept { return restVk; }
    double gain() const noexcept { return gainAtRest; } // small-signal, cathode fully bypassed

    double process (double vin) noexcept
    {
        // coupling capacitor into the grid leak, and the grid pulling current once it is above
        // the cathode (through the stopper and the grid's own resistance, about 1.5 kOhm)
        // The grid starts to conduct softly a little below the cathode (a diode's knee, here a
        // smooth curve about 0.3 V wide, so the corner does not fold back as digital grit).
        const double gc = parts.cCoupling / dt, gl = 1.0 / parts.rGridLeak;
        const double gx = 1.0 / (parts.rStopper + 1500.0), knee = 0.3;
        const double drive = gc * (vin - vc);
        double a = lastA, ig = 0.0;
        for (int it = 0; it < 3; ++it)
        {
            const double x = a - vk, r = std::sqrt (x * x + knee * knee);
            ig = 0.5 * (x + r) * gx;
            const double f = (gc + gl) * a + ig - drive;
            const double df = gc + gl + 0.5 * (1.0 + x / r) * gx;
            a -= f / df;
        }
        {
            const double x = a - vk;
            ig = 0.5 * (x + std::sqrt (x * x + knee * knee)) * gx;
        }
        lastA = a;
        vc = vin - a;
        // the grid itself: the stopper's drop, then its lowpass with the input capacitance
        const double vgRaw = a - ig * parts.rStopper;
        const double v = (vgRaw - miller) * millerG;
        const double vg = v + miller;
        miller = vg + v;

        // the cathode: its capacitor and resistor against the plate current, one Newton step
        double slope;
        double vp = line->plate (vg - vk, slope);
        const double ip0 = (supply - vp) / rPlate;
        const double gm = std::max (0.0, -slope / rPlate); // dIp/dVgk (the plate falls as the grid rises)
        const double gck = parts.cCathode / dt;
        vk = (ip0 + gm * vk + gck * vk) / (1.0 / parts.rCathode + gck + gm);
        vp = line->plate (vg - vk, slope);
        return vp - restVp;
    }

private:
    StageParts parts;
    Triode tube;
    const LoadLine* line = nullptr;
    double supply = 300.0, rPlate = 100000.0;
    double dt = 1.0 / 384000.0;
    double vc = 0.0, vk = 0.0, miller = 0.0, millerG = 0.5, lastA = 0.0;
    double restVk = 1.0, restVp = 200.0, gainAtRest = 60.0;
};
} // namespace rh::tube
