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

// The current through two diodes back to back, and its slope. `up` and `down` are how many
// diodes sit in series each way (one and two clip lopsided, as some drive pedals do).
inline void diodePair (const Diode& d, double v, double& i, double& g, double up = 1.0, double down = 1.0) noexcept
{
    const double nu = up * d.nvt, nd = down * d.nvt;
    // single precision is plenty here (one part in ten million) and several times quicker, and
    // the two exponentials do not wait on each other, which is quicker than one and a division
    const double e = (double) std::exp ((float) std::clamp (v / nu, -60.0, 60.0));
    const double ie = (double) std::exp ((float) std::clamp (-v / nd, -60.0, 60.0));
    i = d.is * (e - ie);
    g = d.is * (e / nu + ie / nd);
}

// A smooth ceiling and floor: what an amplifier's output does as it nears its supply. Within
// `knee` volts of either limit it bends over instead of stopping dead.
// (A hyperbola: smooth all the way, and cheap, two square roots.)
inline double softRails (double v, double lo, double hi, double knee) noexcept
{
    const double a = v - hi, k2 = knee * knee;
    v = hi + 0.5 * (a - std::sqrt (a * a + k2));
    const double b = v - lo;
    return lo + 0.5 * (b + std::sqrt (b * b + k2));
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
    double up = 1.0, down = 1.0; // diodes in series each way
    double v = 0.0, vPrev = 0.0;
    void setSteps (int newtonSteps) noexcept { iterations = newtonSteps; tolerance = newtonTolerance (newtonSteps); }
    void reset() noexcept { v = vPrev = 0.0; }
    double process (double u) noexcept
    {
        double x = v + (v - vPrev);
        for (int it = 0; it < iterations; ++it)
        {
            double id, gd;
            diodePair (d, x, id, gd, up, down);
            const double h = (x - u) / r + id;
            const double step = h / (1.0 / r + gd);
            const double limit = 4.0 * d.nvt * std::max (up, down) + 0.1 * std::abs (x);
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
    double railDown = 4.0, railKnee = 0.0; // a knee above 0 bends into the rails (rail up, railDown down) instead of stopping
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
        out = railKnee > 0.0 ? softRails (out, -railDown, rail, railKnee) : std::clamp (out, -rail, rail);
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
// of an inverting gain stage (470 kOhm), coupling caps between them, the scooped tone network,
// and a recovery stage. Sustain sets how much reaches the first clipping stage. The parts are
// held to the published measurements of the real circuit: the boost about 17 dB, the clipping
// stages about 23 and 25 dB (the transistors' own limited gain, not the ideal 470k ratio),
// each stage's passband 55 Hz to 1.8 kHz and 94 Hz to 1.2 kHz, the boost's own Miller rolloff
// near 1.2 kHz, the recovery stage 13 dB. Those rolloffs are what make it smooth, not fizzy.
struct BigFuzz
{
    OnePoleD inHp, boostLp, hpA, hpB, outHp;
    DiodeFeedback a, b;
    FuzzToneStack stack;
    double fs = 0, sustain = 0.5;
    void prepare (double f, int newtonSteps)
    {
        fs = f;
        inHp.set (fs, 20.0);
        boostLp.set (fs, 1200.0);
        hpA.set (fs, 55.0);
        hpB.set (fs, 94.0);
        outHp.set (fs, 16.0);
        a.prepare (fs, 1.0 / (kTwoPi * 470000.0 * 1780.0), silicon, newtonSteps);
        b.prepare (fs, 1.0 / (kTwoPi * 470000.0 * 1170.0), silicon, newtonSteps);
        a.setResistance (470000.0);
        b.setResistance (470000.0);
        stack.prepare (fs);
        reset();
    }
    void reset() { inHp.reset(); boostLp.reset(); hpA.reset(); hpB.reset(); outHp.reset(); a.reset(); b.reset(); stack.reset(); }
    void set (double drive, double tone)
    {
        sustain = 0.03 + 0.97 * audioTaper (drive);
        stack.setTone (tone);
    }
    double process (double u)
    {
        // the input transistor: about 17 dB, rounding off near its supply
        const double boost = 2.0 * (double) fastTanh ((float) (3.4 * boostLp.lp (inHp.hp (u))));
        // input resistors that give each stage its measured gain: 470k / 14 and 470k / 18
        const double sa = -a.process (hpA.hp (boost * sustain) / 33300.0);
        const double sb = -b.process (hpB.hp (sa) / 26400.0);
        const double tone = stack.process (sb);
        return outHp.hp (tone) * 4.5; // recovery stage, 13 dB
    }
};

// A transistor gain stage, common emitter: the base biased by two resistors and fed through a
// coupling capacitor, the emitter to ground through a resistor with a capacitor (and a gain
// resistor in series with it) across it, the collector to the supply through a resistor. The
// base-emitter junction is a diode (Ebers-Moll: the collector current is beta times the base
// current), so the stage squeezes one half of the wave more than the other, and a big note
// charges the coupling capacitor and shifts the bias for a moment after, as the real ones do.
// Two unknowns each sample (base and emitter), solved together with Newton's method; the
// collector runs into the emitter (saturation) and the supply with a short knee. Written for
// an NPN; a PNP is the same with every voltage turned over.
struct TransistorStage
{
    // parts
    double vcc = 9.0, rbTop = 470e3, rbBottom = 68e3, rc = 10e3, re = 3900.0;
    double cIn = 5e-9, cE = 47e-6, rGain = 0.0; // cE = 0: no bypass; rGain in series with cE
    double isT = 1e-14, nvt = 0.0259, beta = 200.0;
    bool pnp = false;
    int iterations = 6;
    double tolerance = 1.0e-5;
    // state
    double dt = 1.0 / 384000.0;
    double vIn = 0.0, vb = 0.0, ve = 0.0, vCap = 0.0; // vIn: across the input capacitor; vCap: across cE
    double vth = 0.0, gth = 0.0, rest = 0.0;          // the bias network as one source and conductance; resting collector

    void prepare (double fs, int newtonSteps) noexcept
    {
        dt = 1.0 / fs; iterations = newtonSteps; tolerance = newtonTolerance (newtonSteps);
        reset();
    }

    // back to the resting point: the emitter where its resistor carries the current the base
    // allows, nothing flowing in the capacitors
    void reset() noexcept
    {
        vth = vcc * rbBottom / (rbTop + rbBottom);
        gth = 1.0 / rbTop + 1.0 / rbBottom;
        double lo = 0.0, hi = vth;
        for (int it = 0; it < 100; ++it)
        {
            const double v = 0.5 * (lo + hi); // trial emitter voltage
            const double ib = v / re / (beta + 1.0);
            const double vbNeed = v + nvt * std::log (ib * beta / isT + 1.0); // the base this emitter needs
            const double vbGot = vth - ib / gth;                              // what the bias network gives
            if (vbNeed < vbGot) lo = v; else hi = v;
        }
        ve = 0.5 * (lo + hi);
        const double ib = ve / re / (beta + 1.0);
        vb = vth - ib / gth;
        vCap = ve;
        vIn = -vb;
        rest = collector (beta * ib);
    }
    double restingCollector() const noexcept { return rest; }

    double collector (double ic) const noexcept
    {
        // the collector can not go below the emitter (plus a tenth of a volt) or above the supply
        return softRails (vcc - ic * rc, ve + 0.1, vcc, 0.05);
    }

    // input volts in, the collector's swing out (its resting voltage taken off)
    double process (double u) noexcept
    {
        if (pnp) u = -u;
        const double gIn = cIn / dt;
        const double gSeries = cE > 0.0 ? 1.0 / (rGain + dt / cE) : 0.0; // the emitter capacitor leg
        const double k = isT / beta;
        double b = vb, e = ve;
        for (int it = 0; it < iterations; ++it)
        {
            const double ex = (double) std::exp ((float) std::clamp ((b - e) / nvt, -60.0, 60.0));
            const double ib = k * (ex - 1.0), gd = k * ex / nvt;
            // base: what comes through the input capacitor = the bias network + the base current
            const double f1 = gIn * (u - b - vIn) - (b - vth) * gth - ib;
            // emitter: base and collector current = its resistor + the capacitor leg
            const double f2 = (beta + 1.0) * ib - e / re - gSeries * (e - vCap);
            const double a11 = -gIn - gth - gd, a12 = gd;
            const double a21 = (beta + 1.0) * gd, a22 = -(beta + 1.0) * gd - 1.0 / re - gSeries;
            const double det = a11 * a22 - a12 * a21;
            double db = (f1 * a22 - f2 * a12) / det, de = (a11 * f2 - a21 * f1) / det;
            // keep each step within a few thermal voltages: the junction is exponential
            const double big = std::max (std::abs (db), std::abs (de));
            const double limit = 4.0 * nvt + 0.1 * std::abs (b);
            if (big > limit) { db *= limit / big; de *= limit / big; }
            b -= db; e -= de;
            if (big < tolerance) break;
        }
        const double ib = k * ((double) std::exp ((float) std::clamp ((b - e) / nvt, -60.0, 60.0)) - 1.0);
        // the capacitors move on
        vIn = u - b;
        if (cE > 0.0) vCap += gSeries * (e - vCap) * dt / cE;
        vb = b; ve = e;
        const double out = collector (beta * ib) - rest;
        return pnp ? -out : out;
    }
};

// A second-order filter (state variable, trapezoidal) for the bells and shelves of a pedal's
// equaliser. Gains in dB.
struct FilterD
{
    enum class Kind { bell, lowShelf, highShelf };
    double g = 0.1, k = 1.0, s1 = 0.0, s2 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    void set (double fs, Kind kind, double hz, double q, double db) noexcept
    {
        const double a = std::pow (10.0, db / 40.0);
        g = std::tan (kPi * std::clamp (hz, 1.0, 0.49 * fs) / fs);
        k = 1.0 / q;
        if (kind == Kind::bell) { k = 1.0 / (q * a); m0 = 1.0; m1 = k * (a * a - 1.0); m2 = 0.0; }
        else if (kind == Kind::lowShelf) { g /= std::sqrt (a); m0 = 1.0; m1 = k * (a - 1.0); m2 = a * a - 1.0; }
        else { g *= std::sqrt (a); m0 = a * a; m1 = k * (1.0 - a) * a; m2 = 1.0 - a * a; }
    }
    void reset() noexcept { s1 = s2 = 0.0; }
    double process (double x) noexcept
    {
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = x - s2;
        const double v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0 * v1 - s1;
        s2 = 2.0 * v2 - s2;
        return m0 * x + m1 * v1 + m2 * v2;
    }
};

// The clean boost: one silicon transistor, 430 kOhm and 43 kOhm bias, 10 kOhm collector, 390 Ohm
// emitter left unbypassed (about 27 dB), 100 nF in and out, and a 100 kOhm volume pot. Clean
// until a hot pickup digs in, then it rounds off one side first. Drive is the pot (at zero it
// gives the guitar's own level back); tone is a gentle treble shelf after it.
struct CleanBoost
{
    TransistorStage q;
    OnePoleD outHp, shelf;
    double pot = 1.0, treble = 1.0;
    void prepare (double fs, int newtonSteps)
    {
        q.rbTop = 430e3; q.rbBottom = 43e3; q.rc = 10e3; q.re = 390.0; q.cE = 0.0;
        q.cIn = 100e-9; q.isT = 1e-14; q.nvt = 0.0259; q.beta = 250.0;
        q.prepare (fs, newtonSteps);
        outHp.set (fs, 1.0 / (kTwoPi * 100e3 * 100e-9));
        shelf.set (fs, 3000.0);
        reset();
    }
    void reset() { q.reset(); outHp.reset(); shelf.reset(); }
    void set (double drive, double tone)
    {
        pot = std::pow (10.0, (-27.0 + 22.0 * drive) / 20.0);
        treble = std::pow (10.0, (tone - 0.5) * 12.0 / 20.0);
    }
    double process (double u)
    {
        // the stage inverts; turned back so the boost keeps the guitar's polarity
        const double y = -outHp.hp (q.process (u)) * pot;
        return y + (treble - 1.0) * shelf.hp (y);
    }
};

// The treble booster: one germanium PNP transistor, 470 kOhm and 68 kOhm bias, 10 kOhm
// collector, 3.9 kOhm emitter with 47 uF across it, fed through a small input capacitor that
// passes only the top of the guitar, and 10 nF out. Drive is a gain pot in series with the
// emitter capacitor (fully up is the original, all of its gain; down, about 8 dB is left); tone picks the input
// capacitor from 2.2 nF (thin and cutting) to 47 nF (full range), the way builders swap it.
struct TrebleBooster
{
    TransistorStage q;
    OnePoleD outHp;
    void prepare (double fs, int newtonSteps)
    {
        q.pnp = true;
        q.rbTop = 470e3; q.rbBottom = 68e3; q.rc = 10e3; q.re = 3900.0; q.cE = 47e-6;
        q.isT = 6.6e-7; q.nvt = 1.3 * 0.0259; q.beta = 90.0;
        q.prepare (fs, newtonSteps);
        outHp.set (fs, 1.0 / (kTwoPi * 1.0e6 * 10e-9));
        reset();
    }
    void reset() { q.reset(); outHp.reset(); }
    // only the parts change: the transistor keeps playing where it is
    void set (double drive, double tone)
    {
        // a gain pot whose taper adds the same number of decibels each step: from the bare
        // emitter resistor (about 8 dB) to fully bypassed (about 37 dB)
        const double gain = std::pow (10.0, (8.0 + 29.0 * drive) / 20.0);
        const double z = std::max (1.0, q.rc / gain - 140.0); // what the emitter must look like
        q.rGain = z < q.re - 1.0 ? std::min (1.0e6, z * q.re / (q.re - z)) : 1.0e6;
        q.cIn = 2.2e-9 * std::pow (47.0 / 2.2, tone);
    }
    // (the output volume at noon takes about 10 dB back off)
    double process (double u) { return -0.32 * outHp.hp (q.process (u)); }
};

// The blues drive: two discrete op-amp stages on 9 V, their gain set together by one dual pot
// (up to about 27 and 21 dB), lopsided silicon diodes between them (one diode one way, two in
// series the other, behind 2.2 kOhm), and an active treble control. The op-amps run into their
// rails softly and unevenly, which is most of this pedal's open, amp-like break-up.
struct BluesDrive
{
    OnePoleD inHp, couple, shelf, top, out;
    OpAmpNonInverting a, b;
    DiodeShunt clip;
    double treble = 1.0;
    void prepare (double fs, int newtonSteps)
    {
        inHp.set (fs, 1.0 / (kTwoPi * 100e3 * 22e-9));
        couple.set (fs, 1.0 / (kTwoPi * 10e3 * 1e-6));
        shelf.set (fs, 1600.0);
        top.set (fs, 7500.0);
        out.set (fs, 1.0 / (kTwoPi * 10e3 * 1e-6)); // the output capacitor: no DC leaves the pedal
        for (auto* s : { &a, &b })
        {
            s->prepare (fs);
            s->cf = 100e-12; s->r2 = 1.0e9; s->c2 = 1e-12;
            s->ft = 3.0e6; s->slew = 2.0e6;
            s->rail = 3.4; s->railDown = 4.1; s->railKnee = 0.35;
        }
        a.r1 = 4700.0; a.c1 = 0.47e-6;
        b.r1 = 10000.0; b.c1 = 0.22e-6;
        clip.r = 2200.0; clip.d = silicon; clip.up = 1.0; clip.down = 2.0; clip.setSteps (newtonSteps);
        reset();
    }
    void reset() { inHp.reset(); couple.reset(); shelf.reset(); top.reset(); out.reset(); a.reset(); b.reset(); clip.reset(); }
    void set (double drive, double tone)
    {
        const double t = audioTaper (drive);
        a.rf = 100.0 + 100e3 * t;
        b.rf = 100.0 + 100e3 * t;
        treble = std::pow (10.0, (tone - 0.5) * 24.0 / 20.0);
    }
    double process (double u)
    {
        const double s1 = a.process (inHp.hp (u));
        const double s2 = b.process (couple.hp (clip.process (s1)));
        return out.hp (top.lp (s2 + (treble - 1.0) * shelf.hp (s2)));
    }
};

// The metal box: a first op-amp stage that lifts the middle (x11 from about 160 Hz, its
// feedback capacitor rolling off above 1.6 kHz), a second with up to x1000 gain and silicon
// diodes in its feedback, silicon diodes to ground behind 10 kOhm with 3.3 nF taking the fizz
// off what reaches them (about 4.8 kHz), and the equaliser that
// makes these pedals: a low shelf, a mid bell and a high shelf. Tone sweeps from mid-heavy and
// dark to the deep mid scoop and bright top this kind of pedal is set to most of the time.
struct MetalBox
{
    OnePoleD inHp, gainLeg, preClip;
    OpAmpNonInverting pre;
    DiodeFeedback fb;
    DiodeShunt clip;
    FilterD low, mid, high;
    double fs = 0;
    void prepare (double f, int newtonSteps)
    {
        fs = f;
        inHp.set (fs, 100.0);
        pre.prepare (fs);
        pre.rf = 100e3; pre.cf = 1e-9; pre.r1 = 10e3; pre.c1 = 100e-9; pre.r2 = 1.0e9; pre.c2 = 1e-12;
        pre.ft = 3.0e6; pre.slew = 0.5e6; pre.rail = 4.0; pre.railDown = 4.0; pre.railKnee = 0.3;
        gainLeg.set (fs, 1.0 / (kTwoPi * 1000.0 * 1e-6));
        fb.prepare (fs, 100e-12, silicon, newtonSteps);
        clip.r = 10e3; clip.d = silicon; clip.setSteps (newtonSteps);
        preClip.set (fs, 1.0 / (kTwoPi * 10e3 * 3.3e-9));
        low.set (fs, FilterD::Kind::lowShelf, 120.0, 0.7, 3.0);
        reset();
    }
    void reset() { inHp.reset(); gainLeg.reset(); preClip.reset(); pre.reset(); fb.reset(); clip.reset(); low.reset(); mid.reset(); high.reset(); }
    void set (double drive, double tone)
    {
        fb.setResistance (1000.0 + 1.0e6 * audioTaper (drive));
        mid.set (fs, FilterD::Kind::bell, 650.0, 0.7, 4.0 - 18.0 * tone);
        high.set (fs, FilterD::Kind::highShelf, 3500.0, 0.6, -6.0 + 14.0 * tone);
    }
    double process (double u)
    {
        const double p = pre.process (inHp.hp (u));
        const double d = softRails (p + fb.process (gainLeg.hp (p) / 1000.0), -4.0, 4.0, 0.3);
        // a recovery stage after the clipper, then the equaliser
        // (the output volume at noon takes about 10 dB back off)
        return high.process (mid.process (low.process (clip.process (preClip.lp (d)) * 1.25)));
    }
};

} // namespace rh::circuit
