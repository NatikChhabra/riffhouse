// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"

// Parts of real pedal circuits, solved as circuits rather than imitated with curves. Each block
// is the equation of a small piece of a schematic (a diode pair in an op-amp's feedback loop,
// diodes to ground, an op-amp with its gain network, a passive tone network), stepped through
// time with the trapezoidal or backward Euler rule and Newton's method where diodes make it
// nonlinear. The same code runs in the app (oversampled) and in the pedal lab, which runs it
// sixteen times faster with more Newton steps as the reference.
//
// Voltages are volts. A guitar into a pedal swings about half a volt when played hard; the
// drive pedal maps full scale to kVoltsPerUnit volts.
namespace rh::circuit
{
constexpr double kVoltsPerUnit = 2.0;

struct Diode
{
    double is, nvt; // saturation current, emission coefficient times thermal voltage
};
inline constexpr Diode silicon { 2.52e-9, 1.752 * 0.02585 };   // a common small-signal silicon diode
inline constexpr Diode germanium { 2.0e-7, 1.30 * 0.02585 };   // a germanium point-contact diode
inline constexpr Diode led { 2.0e-17, 1.9 * 0.02585 };         // a red LED: clips later, harder

// When Newton may stop. Near the answer each step squares the error: for a diode what is left
// after a step of s volts is about s squared over twice its thermal voltage (s = 10 microvolts
// leaves about a nanovolt, 180 dB under a volt). The lab's reference, given many more steps,
// solves to the last digit instead.
inline double newtonTolerance (int newtonSteps) noexcept { return newtonSteps > 8 ? 1.0e-10 : 1.0e-5; }

// The current through two diodes back to back, and its slope.
inline void diodePair (const Diode& d, double v, double& i, double& g) noexcept
{
    const double a = std::clamp (v / d.nvt, -60.0, 60.0);
    // single precision is plenty here (one part in ten million) and several times quicker
    // (the two exponentials do not wait on each other, which is quicker than one and a division)
    const float af = (float) a;
    const double e = (double) std::exp (af), ie = (double) std::exp (-af);
    i = d.is * (e - ie);
    g = d.is * (e + ie) / d.nvt;
}

// An audio-taper pot: about a tenth of its travel's worth of resistance at half way.
inline double audioTaper (double t) noexcept
{
    t = std::clamp (t, 0.0, 1.0);
    return (std::exp (4.6 * t) - 1.0) / (std::exp (4.6) - 1.0);
}

// A first-order lowpass in double precision (trapezoidal, so the cutoff is exact).
struct OnePoleD
{
    double G = 0.5, s = 0.0;
    void set (double fs, double hz) noexcept
    {
        hz = std::clamp (hz, 0.5, 0.49 * fs);
        const double g = std::tan (kPi * hz / fs);
        G = g / (1.0 + g);
    }
    void reset() noexcept { s = 0.0; }
    double lp (double x) noexcept
    {
        const double v = (x - s) * G;
        const double y = v + s;
        s = y + v;
        return y;
    }
    double hp (double x) noexcept { return x - lp (x); }
};

// A feedback element made of a resistor, a capacitor and two diodes back to back, all in
// parallel, driven by a current. Its voltage is what an op-amp adds to its input when the
// element sits in the op-amp's feedback loop (the green overdrive), or what an inverting
// stage puts out (the big fuzz's clipping stages). The diodes never stop the voltage rising,
// they only slow it to a logarithm: that is the soft, touch-sensitive ceiling of these pedals.
struct DiodeFeedback
{
    double dt = 1.0 / 192000.0, c = 51e-12, g = 1.0 / 51000.0;
    Diode d = silicon;
    int iterations = 6;
    double tolerance = 1.0e-6;
    double v = 0.0, vPrev = 0.0, fPrev = 0.0;

    void prepare (double fs, double capF, const Diode& diode, int newtonSteps) noexcept
    {
        dt = 1.0 / fs; c = capF; d = diode; iterations = newtonSteps; tolerance = newtonTolerance (newtonSteps);
        reset();
    }
    void setResistance (double ohms) noexcept { g = 1.0 / std::max (ohms, 1.0); }
    void reset() noexcept { v = vPrev = 0.0; fPrev = 0.0; }

    // the current that does not go into the capacitor, at voltage x
    double rest (double i, double x, double& slope) const noexcept
    {
        double id, gd;
        diodePair (d, x, id, gd);
        slope = g + gd;
        return i - x * g - id;
    }

    double process (double i) noexcept
    {
        // trapezoidal: c (v - v0) / dt = (f(v) + f(v0)) / 2
        const double k = 2.0 * c / dt;
        const double v0 = v;
        double x = v0 + (v0 - vPrev); // start where the last two answers point: Newton then needs a step or two
        for (int it = 0; it < iterations; ++it)
        {
            double slope;
            const double f = rest (i, x, slope);
            const double h = k * (x - v0) - f - fPrev;
            const double step = h / (k + slope);
            // the diodes' exponential can throw Newton far past the answer: keep each step
            // within a few thermal voltages once they conduct
            const double limit = 4.0 * d.nvt + 0.1 * std::abs (x);
            x -= std::clamp (step, -limit, limit);
            if (std::abs (step) < tolerance) break;
        }
        // at the answer the rule holds, so it gives the rest current without another exponential
        fPrev = k * (x - v0) - fPrev;
        vPrev = v0;
        v = x;
        return x;
    }
};

// Two diodes back to back from a node to ground, fed through a resistor: the hard ceiling of
// the rodent and metal pedals. Static, so it is solved fresh each sample from the last answer.
struct DiodeShunt
{
    double r = 1000.0;
    Diode d = silicon;
    int iterations = 6;
    double tolerance = 1.0e-6;
    double v = 0.0, vPrev = 0.0;
    void setSteps (int newtonSteps) noexcept { iterations = newtonSteps; tolerance = newtonTolerance (newtonSteps); }
    void reset() noexcept { v = vPrev = 0.0; }
    double process (double u) noexcept
    {
        double x = v + (v - vPrev);
        for (int it = 0; it < iterations; ++it)
        {
            double id, gd;
            diodePair (d, x, id, gd);
            const double h = (x - u) / r + id;
            const double step = h / (1.0 / r + gd);
            const double limit = 4.0 * d.nvt + 0.1 * std::abs (x);
            x -= std::clamp (step, -limit, limit);
            if (std::abs (step) < tolerance) break;
        }
        vPrev = v;
        v = x;
        return x;
    }
};

// An op-amp wired as a non-inverting amplifier: from its output to its minus input a resistor
// and a capacitor in parallel, from the minus input to ground up to two resistor-capacitor legs.
// The op-amp itself has a gain that falls with frequency (its gain-bandwidth), a top speed for
// its output (slew rate) and supply rails. Turned up, it runs out of bandwidth before anything
// clips: that is a big part of the rodent pedal's sound. Backward Euler, solved exactly each
// sample, since all of it is linear except the speed limit and the rails.
struct OpAmpNonInverting
{
    double dt = 1.0 / 192000.0;
    double rf = 100000.0, cf = 100e-12;
    double r1 = 47.0, c1 = 2.2e-6, r2 = 560.0, c2 = 4.7e-6;
    double ft = 1.0e6, slew = 0.3e6, rail = 4.0;
    double vo = 0.0, vc1 = 0.0, vc2 = 0.0, vcf = 0.0;

    void prepare (double fs) noexcept { dt = 1.0 / fs; reset(); }
    void reset() noexcept { vo = vc1 = vc2 = vcf = 0.0; }

    double process (double u) noexcept
    {
        // legs to ground: a resistor in series with a capacitor, as a conductance and a source
        const double gl1 = 1.0 / (r1 + dt / c1), gl2 = 1.0 / (r2 + dt / c2);
        // the feedback element: resistor plus capacitor companion
        const double gf = 1.0 / rf + cf / dt;
        const double jf = cf / dt * vcf; // the capacitor's memory, pushing current from out to minus
        // minus input: (vo - vm) gf - jf = (vm - vc1) gl1 + (vm - vc2) gl2   =>   vm = a vo + b
        const double den = gf + gl1 + gl2;
        const double a = gf / den, b = (-jf + vc1 * gl1 + vc2 * gl2) / den;
        // the op-amp integrates its input difference: vo = vo0 + w dt (u - vm)
        const double w = kTwoPi * ft * dt;
        double out = (vo + w * (u - b)) / (1.0 + w * a);
        out = std::clamp (out, vo - slew * dt, vo + slew * dt);
        out = std::clamp (out, -rail, rail);
        const double vm = a * out + b;
        // the capacitors' new voltages
        vc1 += (vm - vc1) * gl1 * dt / c1;
        vc2 += (vm - vc2) * gl2 * dt / c2;
        vcf = out - vm;
        vo = out;
        return out;
    }
};

// The big fuzz's tone network: a resistor-capacitor lowpass and a capacitor-resistor highpass
// side by side, with the tone pot between them and the next stage's input on its wiper. At
// noon both halves are down around 1 kHz, which is the famous scooped middle.
struct FuzzToneStack
{
    double dt = 1.0 / 192000.0;
    double r1 = 39000.0, c1 = 10e-9; // lowpass half
    double r2 = 22000.0, c2 = 4e-9;  // highpass half
    double pot = 100000.0, load = 100000.0, t = 0.5;
    double vl = 0.0, vch = 0.0;      // the two capacitor voltages

    void prepare (double fs) noexcept { dt = 1.0 / fs; reset(); }
    void reset() noexcept { vl = vch = 0.0; }
    void setTone (double tone) noexcept { t = std::clamp (tone, 0.01, 0.99); }

    double process (double u) noexcept
    {
        // backward Euler companions: c1 from node L to ground, c2 from input to node H
        // ra: wiper to the highpass end, rb: wiper to the lowpass end; turning up moves toward highpass
        const double ra = (1.0 - t) * pot, rb = t * pot;
        const double gc1 = c1 / dt, gc2 = c2 / dt;
        const double g1 = 1.0 / r1, g2 = 1.0 / r2, ga = 1.0 / std::max (ra, 1.0), gb = 1.0 / std::max (rb, 1.0), gw = 1.0 / load;
        // unknowns L, H, W
        // L: (u - L) g1 = (L - 0) gc1 - gc1 vl + (L - W) gb      (the lowpass end is the low-tone side)
        // H: (u - H) gc2 - gc2 vch = H g2 + (H - W) ga
        // W: (L - W) gb + (H - W) ga = W gw
        const double a11 = g1 + gc1 + gb, a13 = -gb, b1 = u * g1 + gc1 * vl;
        const double a22 = gc2 + g2 + ga, a23 = -ga, b2 = u * gc2 - gc2 * vch;
        const double a31 = -gb, a32 = -ga, a33 = gb + ga + gw;
        // L = (b1 - a13 W) / a11, H = (b2 - a23 W) / a22
        const double w = (0.0 - a31 * b1 / a11 - a32 * b2 / a22) / (a33 - a31 * a13 / a11 - a32 * a23 / a22);
        const double l = (b1 - a13 * w) / a11, h = (b2 - a23 * w) / a22;
        vl = l;
        vch = u - h;
        return w;
    }
};
// ---------------------------------------------------------------------------------------------
// Whole pedals, built from the parts above. Volts in, volts out, knobs 0..1. Each has
// prepare (sample rate, Newton steps), set (drive, tone), reset() and process (volts).

// The green mid-hump overdrive: an op-amp whose feedback loop holds 51 kOhm plus the drive pot,
// 51 pF, and two silicon diodes; to ground from its minus input 4.7 kOhm and 47 nF, so only the
// part of the guitar above about 720 Hz is boosted and clipped, and the rest passes clean.
// Then a fixed 723 Hz lowpass and a tone control that lifts the treble back.
struct GreenDrive
{
    OnePoleD gainLeg, toneLp, toneTop;
    DiodeFeedback fb;
    double fs = 0, rTone = 0.3;
    void prepare (double f, int newtonSteps)
    {
        fs = f;
        gainLeg.set (fs, 1.0 / (kTwoPi * 4700.0 * 47e-9));
        toneLp.set (fs, 1.0 / (kTwoPi * 1000.0 * 0.22e-6));
        toneTop.set (fs, 9000.0);
        fb.prepare (fs, 51e-12, silicon, newtonSteps);
        reset();
    }
    void reset() { gainLeg.reset(); toneLp.reset(); toneTop.reset(); fb.reset(); }
    void set (double drive, double tone)
    {
        fb.setResistance (51000.0 + 500000.0 * audioTaper (drive));
        rTone = 0.05 * std::pow (1.6 / 0.05, tone);
    }
    double process (double u)
    {
        const double i = gainLeg.hp (u) / 4700.0;
        const double clip = std::clamp (u + fb.process (i), -4.0, 4.0);
        const double lp = toneLp.lp (clip);
        return toneTop.lp (lp + rTone * (clip - lp));
    }
};

// The rodent distortion: an old op-amp at up to 2000x gain, its gain network 47 Ohm and
// 2.2 uF plus 560 Ohm and 4.7 uF to ground, 100 kOhm pot and 100 pF across. It runs out of
// bandwidth and speed before anything clips. Then two silicon diodes to ground behind 1 kOhm,
// and a filter: 1.5 kOhm plus the 100 kOhm filter pot into 3.3 nF.
struct RodentDist
{
    OpAmpNonInverting amp;
    OnePoleD coupling, filter;
    DiodeShunt clip;
    double fs = 0;
    void prepare (double f, int newtonSteps)
    {
        fs = f;
        amp.prepare (fs);
        coupling.set (fs, 34.0);
        clip.setSteps (newtonSteps);
        reset();
    }
    void reset() { amp.reset(); coupling.reset(); filter.reset(); clip.reset(); }
    void set (double drive, double tone)
    {
        amp.rf = 100.0 + 100000.0 * audioTaper (drive);
        filter.set (fs, 1.0 / (kTwoPi * (1500.0 + (1.0 - tone) * 100000.0) * 3.3e-9));
    }
    double process (double u)
    {
        const double a = coupling.hp (amp.process (u));
        return filter.lp (clip.process (a));
    }
};

// The big fuzz: a transistor boost, two clipping stages each with two diodes in the feedback
// of an inverting gain stage (470 kOhm, 100 pF), coupling caps between them, the scooped tone
// network, and a recovery stage. Sustain sets how much reaches the first clipping stage.
struct BigFuzz
{
    OnePoleD inHp, hpA, hpB, outHp;
    DiodeFeedback a, b;
    FuzzToneStack stack;
    double fs = 0, sustain = 0.5;
    void prepare (double f, int newtonSteps)
    {
        fs = f;
        inHp.set (fs, 70.0);
        hpA.set (fs, 1.0 / (kTwoPi * 10000.0 * 100e-9));
        hpB.set (fs, 1.0 / (kTwoPi * 10000.0 * 100e-9));
        outHp.set (fs, 40.0);
        a.prepare (fs, 100e-12, silicon, newtonSteps);
        b.prepare (fs, 100e-12, silicon, newtonSteps);
        a.setResistance (470000.0);
        b.setResistance (470000.0);
        stack.prepare (fs);
        reset();
    }
    void reset() { inHp.reset(); hpA.reset(); hpB.reset(); outHp.reset(); a.reset(); b.reset(); stack.reset(); }
    void set (double drive, double tone)
    {
        sustain = 0.03 + 0.97 * audioTaper (drive);
        stack.setTone (tone);
    }
    double process (double u)
    {
        // the input transistor: about 20 dB, rounding off near its supply
        const double boost = 2.0 * (double) fastTanh ((float) (5.0 * inHp.hp (u)));
        const double sa = -a.process (hpA.hp (boost * sustain) / 10000.0);
        const double sb = -b.process (hpB.hp (sa) / 10000.0);
        const double tone = stack.process (sb);
        return outHp.hp (tone) * 6.0; // recovery stage
    }
};

} // namespace rh::circuit
