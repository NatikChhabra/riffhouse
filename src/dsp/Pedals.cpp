// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Pedals.h"

namespace rh
{
//==============================================================================
// HumCanceller
void HumCanceller::prepare (double sampleRate)
{
    sr = sampleRate;
    mu = 2.0 / (0.12 * sr);                         // the model settles in about 0.12 s of quiet
    stepLength = std::max (1, (int) (0.02 * sr));    // one mains cycle
    floorRise = std::pow (10.0, 0.02 * 1.0 / 10.0);  // the quietest-lately creeps up 1 dB a second
    holdSamples = (int) (0.06 * sr);
    letGoAfter = (int) (4.0 * sr);
    useStep = 1.0 / (2.0 * sr);
    setMains (mainsHz);
}

void HumCanceller::reset()
{
    forget();
    stepFill = 0; stepPower = inPower = 0.0; floorPower = inFloor = floorPeak = -1.0; stepPeak = 0.0; freezeAbove = 0.0f;
    levelAt = levelsHeard = 0;
}

void HumCanceller::forget()
{
    for (int k = 0; k < kHarmonics; ++k) a[k] = b[k] = 0.0;
    zr = 1.0; zi = 0.0;
    adaptHold = holdSamples; frozenFor = 0; use = 1.0;
    haveTheta = false;
}

void HumCanceller::setMains (int hz)
{
    mainsHz = hz;
    w0 = kTwoPi * hz / sr;
    wCorr = 0.0;
    rr = std::cos (w0); ri = std::sin (w0);
    forget(); // what it knows about how quiet the guitar gets still holds
}

float HumCanceller::process (float x) noexcept
{
    if (mainsHz <= 0) return x;
    const double nr = zr * rr - zi * ri;
    zi = zr * ri + zi * rr;
    zr = nr;
    double hr = zr, hi = zi, h = 0.0;
    for (int k = 0; k < kHarmonics; ++k)
    {
        cs[k] = hr; sn[k] = hi;
        h += a[k] * hr + b[k] * hi;
        const double t = hr * zr - hi * zi;
        hi = hr * zi + hi * zr;
        hr = t;
    }
    const double err = x - h;
    // anything clearly over the quiet floor is playing: stop learning at once, and for a moment after
    if (floorPower < 0.0 || std::abs (err) > freezeAbove) adaptHold = holdSamples;
    else if (adaptHold > 0) --adaptHold;
    stepPower += err * err;
    stepPeak = std::max (stepPeak, std::abs (err));
    inPower += (double) x * x;
    if (++stepFill >= stepLength) stepDone();
    if (adaptHold == 0)
    {
        const double g = mu * err;
        for (int k = 0; k < kHarmonics; ++k) { a[k] += g * cs[k]; b[k] += g * sn[k]; }
        frozenFor = 0;
    }
    else if (frozenFor < letGoAfter) ++frozenFor;
    // a long phrase with no quiet in it: the mains may have drifted, so let go of the model slowly
    const double want = frozenFor >= letGoAfter ? 0.0 : 1.0;
    use += std::clamp (want - use, -useStep, 8.0 * useStep);
    return (float) (x - use * h);
}

void HumCanceller::stepDone()
{
    const double p = stepPower / (double) stepLength, pin = inPower / (double) stepLength;
    stepPower = inPower = 0.0;
    stepFill = 0;
    if (floorPower < 0.0 || p < floorPower) floorPower = p;
    else floorPower *= floorRise;
    if (floorPeak < 0.0 || stepPeak < floorPeak) floorPeak = stepPeak;
    else floorPeak *= std::sqrt (floorRise);
    const double peakNow = stepPeak;
    stepPeak = 0.0;
    if (inFloor < 0.0 || pin < inFloor) inFloor = pin;
    else inFloor *= floorRise;
    // steadiness is judged on what comes in: the residual drops while it learns, the input does not
    levels[levelAt] = (float) (10.0 * std::log10 (pin + 1.0e-20));
    levelAt = (levelAt + 1) % kSteady;
    levelsHeard = std::min (levelsHeard + 1, kSteady);
    float lo = levels[0], hi = levels[0];
    for (float l : levels) { lo = std::min (lo, l); hi = std::max (hi, l); }
    if (levelsHeard < kSteady || hi - lo > 2.0f) adaptHold = std::max (adaptHold, stepLength);
    else if (hi - lo < 1.5f && hi < -40.0f && p > 4.0 * floorPower)
    {
        // the same level for 300 ms and not loud: the room got noisier (a dimmer, a laptop on its
        // charger). A guitar note fades more than that. Take this as the new quiet.
        floorPower = p;
        floorPeak = peakNow;
        inFloor = std::max (inFloor, pin);
        adaptHold = 0;
    }
    // never louder than the quiet input it hides in: a note learnt by mistake cannot ring on
    double model = 0.0;
    for (int k = 0; k < kHarmonics; ++k) model += 0.5 * (a[k] * a[k] + b[k] * b[k]);
    if (model > 1.5 * inFloor && model > 0.0)
    {
        const double g = std::sqrt (1.5 * std::max (inFloor, 0.0) / model);
        for (int k = 0; k < kHarmonics; ++k) { a[k] *= g; b[k] *= g; }
    }
    const double floorRms = std::sqrt (std::max (floorPower, 1.0e-14));
    // hiss peaks reach 3 to 4 times its level, a buzzing hum 5 or more: twice the quietest
    // 20 ms peak lately covers both, and a note is far louder than either
    freezeAbove = (float) std::max (6.0 * floorRms, 2.0 * floorPeak);
    if (p > 4.0 * floorPower) adaptHold = std::max (adaptHold, holdSamples);
    const double m = std::sqrt (zr * zr + zi * zi);
    zr /= m; zi /= m;

    // The mains drifts (49.9 to 50.1 Hz is normal): while learning, the strongest harmonic's
    // phase turns as fast as the drift. Turn the model's clock to match, so it stays right
    // through a phrase it cannot learn in.
    int best = 0;
    double bestR = 0.0;
    for (int k = 0; k < kHarmonics; ++k)
    {
        const double r = std::hypot (a[k], b[k]);
        if (r > bestR) { bestR = r; best = k; }
    }
    if (adaptHold == 0 && frozenFor == 0 && bestR > 0.2 * floorRms && bestR > 1.0e-6)
    {
        const double theta = std::atan2 (b[best], a[best]);
        if (haveTheta && best == thetaHarmonic)
        {
            double d = theta - lastTheta;
            while (d > kPi) d -= kTwoPi;
            while (d < -kPi) d += kTwoPi;
            const double maxCorr = kTwoPi * 0.6 / sr;
            wCorr = std::clamp (wCorr - 0.05 * d / ((best + 1) * (double) stepLength), -maxCorr, maxCorr);
            rr = std::cos (w0 + wCorr); ri = std::sin (w0 + wCorr);
        }
        lastTheta = theta;
        thetaHarmonic = best;
        haveTheta = true;
    }
    else
        haveTheta = false;
}

//==============================================================================
// InputConditioner
void InputConditioner::prepare (double sampleRate)
{
    sr = sampleRate;
    dc.prepare (sr);
    hum.prepare (sr);
    const double freqs[6] = { 50, 100, 150, 60, 120, 180 };
    for (int i = 0; i < 6; ++i)
        gz[i].coeff = 2.0 * std::cos (kTwoPi * freqs[i] / sr);
    const float amount = fixAmount < 0.0f ? 6.0f : fixAmount;
    fixAmount = -1.0f;
    setFixAmount (amount);
    hiss.prepare (sr);
    activeMains = -1;
    setNotches (0);
    reset();
}

void InputConditioner::reset()
{
    dc.reset();
    hum.reset();
    fixShelf.reset(); fixTop.reset();
    hiss.reset();
    for (auto& g : gz) g.s1 = g.s2 = 0;
    g50 = g60 = total = 0;
    counted = 0;
}

void InputConditioner::set (int humMode, bool inputFix, float fixAmountDb)
{
    mode = humMode;
    fix = inputFix;
    setFixAmount (fixAmountDb);
    int want = 0;
    if (mode == hum50) want = 50;
    else if (mode == hum60) want = 60;
    else if (mode == humAuto) want = detected.load();
    if (want != activeMains)
        setNotches (want);
}

// Input Fix, sized by how much treble the input is missing. A passive pickup is a
// coil of a few henries; into the few kilohms of a PC mic jack it becomes a
// first-order low-pass that starts as low as 300 Hz (measured on a real guitar
// into an onboard mic jack: 20 dB down at 3 kHz against the same kind of guitar
// on an instrument input). The fix is the same curve upside down: it rises 6 dB
// per octave from the corner the amount stands for and levels off at 4.5 kHz,
// where a pickup on a proper input has its own peak and falls away. Above that
// there is mostly hiss, so the top is rolled off the way a pickup and cable do it.
namespace { constexpr double kFixTopHz = 4500.0; }

void InputConditioner::Shelf::set (double sampleRate, double lowHz, double highHz) noexcept
{
    const double k1 = 1.0 / std::tan (kPi * lowHz / sampleRate);
    const double k2 = 1.0 / std::tan (kPi * std::min (highHz, 0.45 * sampleRate) / sampleRate);
    b0 = (1.0 + k1) / (1.0 + k2);
    b1 = (1.0 - k1) / (1.0 + k2);
    a1 = (1.0 - k2) / (1.0 + k2);
}

void InputConditioner::setFixAmount (float db)
{
    db = std::clamp (db, 0.0f, TiltProbe::kMostFixDb);
    if (std::abs (db - fixAmount) < 0.01f) return;
    fixAmount = db;
    fixShelf.set (sr, kFixTopHz / std::pow (10.0, db / 20.0), kFixTopHz);
    // the more is lifted, the more hiss comes up with it: bring the top down to 5.2 kHz by 12 dB
    const double top = 12000.0 + (5200.0 - 12000.0) * std::clamp ((db - 3.0) / 9.0, 0.0, 1.0);
    fixTop.set (Biquad::Type::lowpass, sr, top, 0.85);
}

void InputConditioner::setNotches (int mainsHz)
{
    activeMains = mainsHz;
    hum.setMains (std::max (0, mainsHz));
}

void InputConditioner::analyse (const float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const double s = x[i];
        total += s * s;
        for (auto& g : gz)
        {
            const double s0 = s + g.coeff * g.s1 - g.s2;
            g.s2 = g.s1;
            g.s1 = s0;
        }
    }
    counted += n;

    // evaluate every half second
    if (counted >= (int) (sr * 0.5))
    {
        auto power = [] (const Goertzel& g) { return g.s1 * g.s1 + g.s2 * g.s2 - g.coeff * g.s1 * g.s2; };
        const double p50 = power (gz[0]) + power (gz[1]) + power (gz[2]);
        const double p60 = power (gz[3]) + power (gz[4]) + power (gz[5]);
        const double norm = (double) counted * (double) counted * 0.25; // sine of amplitude A gives A^2 * N^2 / 4
        const double rms2 = total / (double) counted;
        const double hum50 = p50 / norm, hum60 = p60 / norm;

        // only count windows where hum is clearly present and stands out, so a
        // played note near 50/60 Hz harmonics does not fool the detector
        const double floor = 1.0e-10; // about -100 dBFS
        if (std::max (hum50, hum60) > floor && std::max (hum50, hum60) > 0.02 * rms2)
        {
            if (hum50 > 4.0 * hum60) ++votes50;
            else if (hum60 > 4.0 * hum50) ++votes60;
        }
        if (votes50 >= 3 && votes50 > votes60 + 2) detected.store (50);
        else if (votes60 >= 3 && votes60 > votes50 + 2) detected.store (60);
        votes50 = std::min (votes50, 20);
        votes60 = std::min (votes60, 20);

        for (auto& g : gz) g.s1 = g.s2 = 0;
        total = 0;
        counted = 0;
    }
}

void InputConditioner::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
        x[i] = dc.process (x[i]);

    if (mode == humAuto)
    {
        analyse (x, n);
        if (detected.load() != activeMains)
            setNotches (detected.load());
    }

    if (mode != humOff && activeMains > 0)
        for (int i = 0; i < n; ++i)
            x[i] = hum.process (x[i]);

    if (fix)
        for (int i = 0; i < n; ++i)
            x[i] = hiss.process (fixTop.process (fixShelf.process (x[i])));
}

void InputConditioner::HissReducer::prepare (double sampleRate)
{
    sr = sampleRate;
    split.set (Biquad::Type::lowpass, sr, 2200.0, 0.7071);
    envCoeff = (float) std::exp (-1.0 / (0.003 * sr));   // the band's level over about 3 ms
    openCoeff = (float) std::exp (-1.0 / (0.001 * sr));  // opens in about a millisecond
    closeCoeff = (float) std::exp (-1.0 / (0.040 * sr)); // closes over about 40 ms
    stepLength = std::max (1, (int) (0.02 * sr));
    floorPower = -1.0f; // not learnt yet: nothing is turned down
    reset();
}

void InputConditioner::HissReducer::reset()
{
    split.reset();
    env = 0.0f; gain = 1.0f;
    stepPower = 0.0f; stepAt = 0;
    runRef = runSum = 0.0f; runLength = 0;
}

float InputConditioner::HissReducer::floorDb() const noexcept
{
    return floorPower > 0.0f ? 10.0f * std::log10 (floorPower) : -200.0f;
}

float InputConditioner::HissReducer::process (float x) noexcept
{
    // the two halves add back to exactly the input
    const float low = split.process (x), high = x - low;
    const float p = high * high;
    env = p + envCoeff * (env - p);

    // Learn the floor in 20 ms steps. Hiss is steady and notes never are (they fade): 400 ms
    // in which the band stays within a decibel is the floor, up or down. Any quieter step
    // shows the floor is lower, at once.
    stepPower += p;
    if (++stepAt >= stepLength)
    {
        const float step = std::max (stepPower / (float) stepLength, 1.0e-20f);
        stepPower = 0.0f; stepAt = 0;
        if (floorPower > 0.0f && step < floorPower) floorPower = step;
        if (step < runRef * 1.26f && step > runRef * 0.794f)
        {
            runSum += step;
            if (++runLength % 20 == 0) floorPower = runSum / (float) runLength;
        }
        else
        {
            runRef = step; runSum = step; runLength = 1;
        }
    }

    // within 12 dB of the floor, one dB less treble for each dB nearer it (1:2), at most 18 dB
    float target = 1.0f;
    if (floorPower > 0.0f)
    {
        const float knee = floorPower * 15.85f; // +12 dB
        if (env < knee) target = std::max (0.126f, std::sqrt (std::max (env, 1.0e-20f) / knee));
    }
    gain = target + (target > gain ? openCoeff : closeCoeff) * (gain - target);
    return low + gain * high;
}

//==============================================================================
// TiltProbe
void TiltProbe::prepare (double sampleRate)
{
    sr = sampleRate;
    lowBand.set (Biquad::Type::bandpass, sr, 250.0, 0.9);
    // steep sides: on a dull input the lows are 30 dB stronger than this band,
    // and a gentle filter would mostly measure what leaks in from them
    highBand[0].set (Biquad::Type::highpass, sr, 1800.0, 0.5412);
    highBand[1].set (Biquad::Type::highpass, sr, 1800.0, 1.3066);
    highBand[2].set (Biquad::Type::lowpass, sr, 3600.0, 0.5412);
    highBand[3].set (Biquad::Type::lowpass, sr, 3600.0, 1.3066);
    lowBand.reset();
    for (auto& b : highBand) b.reset();
}

void TiltProbe::process (const float* x, int n)
{
    if (restartRequested.exchange (false))
    {
        lowAt.fill (0.0); highAt.fill (0.0); secondsAt.fill (0.0);
        loudest = -1; quietest = kLevels;
        seconds.store (0.0);
        tilt.store (0.0f);
    }
    double sum = 0, lo = 0, hi = 0;
    float pk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float s = x[i];
        const float a = lowBand.process (s);
        float b = s;
        for (auto& f : highBand) b = f.process (b);
        sum += (double) s * s;
        lo += (double) a * a;
        hi += (double) b * b;
        pk = std::max (pk, std::abs (s));
    }
    // Only blocks where something is being played count, and never clipped ones. Playing is
    // judged against this input itself, not a fixed level: a guitar into a PC jack can peak
    // at -35 dBFS, and a fixed -48 dBFS bar left one second of a three-minute take. A block
    // counts when it stands 15 dB over the quietest heard and within 30 dB of the loudest,
    // judged over everything heard so far.
    if (pk > 0.97f || n <= 0) return;
    const double rmsDb = 10.0 * std::log10 (std::max (sum / n, 1.0e-14));
    const int level = std::clamp ((int) std::lround (rmsDb) + kLevels - 1, 0, kLevels - 1);
    lowAt[(size_t) level] += lo;
    highAt[(size_t) level] += hi;
    secondsAt[(size_t) level] += n / sr;
    quietest = std::min (quietest, level);
    loudest = std::max (loudest, level);
    const int from = std::max ({ kLevels - 1 - 80, quietest + 15, loudest - 30 });
    double lowE = 0, highE = 0, secs = 0;
    for (int l = from; l <= loudest; ++l)
    {
        lowE += lowAt[(size_t) l];
        highE += highAt[(size_t) l];
        secs += secondsAt[(size_t) l];
    }
    seconds.store (secs);
    if (lowE > 1e-12)
        tilt.store ((float) (10.0 * std::log10 (std::max (highE, 1e-20) / lowE)));
}

float TiltProbe::tiltGainOf (float amountDb) noexcept
{
    // what the fix's curve does to the two bands, less the quarter of it that real
    // playing does not show (measured: the reading moves 0.7 to 0.85 dB per dB of curve,
    // because the low edge of the high band, where less is lifted, carries most of it)
    const double f1 = kFixTopHz / std::pow (10.0, (double) amountDb / 20.0);
    auto gain = [f1] (double hz) { return 10.0 * std::log10 ((1.0 + (hz / f1) * (hz / f1)) / (1.0 + (hz / kFixTopHz) * (hz / kFixTopHz))); };
    return (float) (0.75 * (gain (2600.0) - gain (250.0)));
}

float TiltProbe::fixAmountFor (float measuredTiltDb, float smallestLossDb) noexcept
{
    // Guitars differ: a dark neck pickup reads 5 dB under the reference with
    // nothing wrong with the input. Past the smallest loss worth mending, find
    // the amount that wins the missing treble back.
    const float deficit = referenceTiltDb - measuredTiltDb;
    if (deficit < smallestLossDb) return 0.0f;
    float lo = 0.0f, hi = kMostFixDb;
    if (tiltGainOf (hi) <= deficit) return hi;
    for (int i = 0; i < 24; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        (tiltGainOf (mid) < deficit ? lo : hi) = mid;
    }
    return std::round (hi * 2.0f) * 0.5f;
}

//==============================================================================
// NoiseGate
void NoiseGate::prepare (double sampleRate)
{
    sr = sampleRate;
    attackCoeff = (float) std::exp (-1.0 / (0.0008 * sr));
    stopCoeff = (float) std::exp (-1.0 / (0.003 * sr)); // 40 dB down in about 14 ms
    holdSamples = (int) (0.025 * sr); // the detector already holds the last 20 ms
    windowLength = std::max (1, (int) (0.1 * sr));
    stepLength = std::max (1, (int) (0.0025 * sr));
    reopenDecay = dbToGain (-0.3f); // per step: what a stop asked for is forgotten in about 100 ms
    floorDb = floorPeakDb = -120.0f; // as if just built: what an earlier run heard is forgotten
    windowPeak = 0.0f;
    windowsHeard = windowAt = windowFill = 0;
    windowPower = 0.0;
    set (-62.0f, 80.0f);
    reset();
}

void NoiseGate::reset()
{
    gain = 0.0f; open = false; stopping = false; holdCounter = 0;
    std::fill (std::begin (steps), std::end (steps), 0.0f);
    stepAt = stepFill = 0;
    stepPeak = heldPeak = recentPeak = earlierPeak = reopenAbove = 0.0f;
}

void NoiseGate::set (float thresholdDb, float releaseMs)
{
    userThrDb = thresholdDb;
    applyThreshold();
    // Release is how long it takes to close: 40 dB down after that many ms. (It used to be the
    // time constant, which took 4.6 times as long, and after a high-gain amp the crunch hung on.)
    releaseCoeff = (float) std::exp (-4.6 / (0.001 * std::max (5.0f, releaseMs) * sr));
}

void NoiseGate::applyThreshold()
{
    float t = userThrDb;
    if (follows && floorDb > -119.0f)
    {
        const float cap = std::clamp (userThrDb + kMostFollowAboveDb, kFollowCapLowDb, kFollowCapHighDb);
        t = std::max (t, std::min (std::max (floorDb + kAboveFloorDb, floorPeakDb + kAbovePeaksDb), cap));
    }
    thrNowDb = t;
    thrOpen = dbToGain (t);
    thrClose = dbToGain (t - 4.0f); // hysteresis stops chattering
}

void NoiseGate::windowDone (float levelDb, float peakDb)
{
    windows[windowAt] = levelDb;
    windowPeaks[windowAt] = peakDb;
    windowAt = (windowAt + 1) % kWindows;
    windowsHeard = std::min (windowsHeard + 1, kWindows);
    // steady to within a decibel and a half for 1.6 s: no string does that, hiss and hum do.
    // The very first floor needs only 0.8 s, so a noisy room is gated almost from the start
    // (a quieter floor heard later is believed at once anyway).
    const int need = floorDb < -119.0f ? kFirstWindows : kWindows;
    if (windowsHeard < need) return;
    float lo = 1000.0f, hi = -1000.0f, sum = 0.0f, peak = -1000.0f;
    for (int k = 1; k <= need; ++k)
    {
        const int i = (windowAt - k + kWindows) % kWindows;
        lo = std::min (lo, windows[i]); hi = std::max (hi, windows[i]); sum += windows[i];
        peak = std::max (peak, windowPeaks[i]);
    }
    if (hi - lo > 1.5f) return;
    const float heard = sum / (float) need;
    if (heard > -40.0f) return;                 // that loud and steady is a sound, not a floor
    if (heard < floorDb) floorDb = std::max (-120.0f, heard); // quieter now: believed at once
    else floorDb = std::min (heard, floorDb < -119.0f ? heard : floorDb + 1.0f);
    if (peak < floorPeakDb) floorPeakDb = std::max (-120.0f, peak);
    else floorPeakDb = std::min (peak, floorPeakDb < -119.0f ? peak : floorPeakDb + 1.0f);
    applyThreshold();
}

void NoiseGate::stepDone()
{
    steps[stepAt] = stepPeak;
    stepAt = (stepAt + 1) % kSteps;
    stepPeak = 0.0f;
    auto back = [this] (int k) { return steps[(stepAt - k + 2 * kSteps) % kSteps]; }; // k = 1 is the step just done
    heldPeak = 0.0f;
    for (int k = 1; k <= 8; ++k) heldPeak = std::max (heldPeak, back (k));    // the last 20 ms
    recentPeak = std::max (back (1), back (2));                               // the last 5 ms
    // how loud it held 22 to 37 ms ago: the quietest of three 5 ms spans, so a pick's click
    // (a few ms) cannot pass for the level of the note
    earlierPeak = std::min ({ std::max (back (10), back (11)), std::max (back (12), back (13)), std::max (back (14), back (15)) });
    reopenAbove *= reopenDecay;
    // a hand on the strings: down 18 dB or more inside about 30 ms, landing near the threshold
    if (open && ! stopping
        && recentPeak * dbToGain (kStopDropDb) < earlierPeak
        && recentPeak < thrOpen * dbToGain (kStopRangeDb))
    {
        open = false;
        stopping = true;
        holdCounter = 0;
        reopenAbove = recentPeak * dbToGain (12.0f); // what rings on after the stop does not open it again
    }
}

void NoiseGate::process (float* x, int n, float* gainsOut)
{
    if (follows)
        for (int i = 0; i < n; ++i)
        {
            windowPower += (double) x[i] * x[i];
            windowPeak = std::max (windowPeak, std::abs (x[i]));
            if (++windowFill >= windowLength)
            {
                windowDone ((float) (10.0 * std::log10 (windowPower / (double) windowLength + 1.0e-12)), gainToDb (windowPeak));
                windowFill = 0;
                windowPower = 0.0;
                windowPeak = 0.0f;
            }
        }
    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (x[i]);
        stepPeak = std::max (stepPeak, a);
        if (++stepFill >= stepLength) { stepFill = 0; stepDone(); }
        const float e = std::max (heldPeak, stepPeak);
        if (a > std::max (thrOpen, reopenAbove)) { open = true; stopping = false; reopenAbove = 0.0f; holdCounter = holdSamples; }
        else if (open && e < thrClose)
        {
            if (holdCounter > 0) --holdCounter;
            else open = false;
        }
        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? attackCoeff : (stopping ? stopCoeff : releaseCoeff);
        gain = undenorm (target + (gain - target) * c);
        x[i] *= gain;
        // after the amp only the closing counts: opening there too would slow the attack
        if (gainsOut != nullptr) gainsOut[i] = open ? 1.0f : gain;
    }
}

//==============================================================================
// Compressor
void Compressor::prepare (double sampleRate)
{
    sr = sampleRate;
    makeupSmooth.prepare (sr, 100.0f);
    mixSmooth.prepare (sr, 20.0f);
    threshSmooth.prepare (sr, 100.0f);
    ratioSmooth.prepare (sr, 100.0f);
    set (5, 4, 5, 10);
    reset();
}

void Compressor::reset()
{
    envDb = -120.0f;
    grDb = 0.0f;
    makeupSmooth.reset (makeupDb);
    mixSmooth.reset (mix);
    threshSmooth.reset (thresholdDb);
    ratioSmooth.reset (ratio);
}

void Compressor::set (float sustain, float attack, float level, float blend)
{
    const float s = clamp01 (sustain / 10.0f);
    thresholdDb = lerp (-12.0f, -42.0f, s);
    ratio = lerp (2.0f, 10.0f, s);
    // auto makeup: about 60% of the reduction a typical note gets
    const float typical = -18.0f;
    const float reduction = std::max (0.0f, (typical - thresholdDb) * (1.0f - 1.0f / ratio));
    makeupDb = 0.6f * reduction + (level / 10.0f - 0.5f) * 24.0f;
    const float atMs = expMap (attack / 10.0f, 0.5f, 40.0f);
    att = (float) std::exp (-1.0 / (0.001 * atMs * sr));
    rel = (float) std::exp (-1.0 / (0.180 * sr));
    mix = clamp01 (blend / 10.0f);
    makeupSmooth.setTarget (makeupDb);
    mixSmooth.setTarget (mix);
    threshSmooth.setTarget (thresholdDb);
    ratioSmooth.setTarget (ratio);
}

void Compressor::follow (float in) noexcept
{
    const float lvl = gainToDb (std::abs (in));
    envDb = lvl > envDb ? lvl + (envDb - lvl) * att : lvl + (envDb - lvl) * rel;
}

void Compressor::listen (const float* x, int n)
{
    for (int i = 0; i < n; ++i) follow (x[i]);
}

void Compressor::process (float* x, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float in = x[i];
        follow (in);
        const float th = threshSmooth.next(), ra = ratioSmooth.next();
        // soft knee of 6 dB
        const float over = envDb - th;
        float gr = 0.0f;
        if (over > 3.0f) gr = over * (1.0f - 1.0f / ra);
        else if (over > -3.0f) gr = (1.0f - 1.0f / ra) * (over + 3.0f) * (over + 3.0f) / 12.0f;
        grDb = gr;
        const float wet = in * dbToGain (makeupSmooth.next() - gr);
        x[i] = lerp (in, wet, mixSmooth.next());
    }
}

//==============================================================================
// Wah: resonant state-variable filter swept 350 Hz to 2.2 kHz.
void Wah::prepare (double sampleRate)
{
    sr = sampleRate;
    env.prepare (sr, 4.0f, 120.0f);
    posSmooth.prepare (sr, 25.0f);
    posSmooth.reset (0.5f);
    reset();
}

void Wah::reset() { ic1 = ic2 = hp = 0.0f; env.reset(); }

void Wah::set (int m, float p, float s, float q)
{
    mode = m;
    pos = clamp01 (p / 10.0f);
    sens = clamp01 (s / 10.0f);
    res = clamp01 (q / 10.0f);
}

void Wah::process (float* x, int n)
{
    if (mode == 2) { processClassic (x, n); return; }
    const float k = 1.0f / lerp (2.0f, 9.0f, res); // damping
    for (int i = 0; i < n; ++i)
    {
        float p = pos;
        if (mode == 1)
        {
            const float e = env.process (x[i]);
            p = clamp01 (std::sqrt (e * lerp (4.0f, 60.0f, sens)));
        }
        posSmooth.setTarget (p);
        const float f = expMap (posSmooth.next(), 350.0f, 2200.0f);
        const float g = (float) std::tan (kPi * f / sr);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = x[i] - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = undenorm (2.0f * v1 - ic1);
        ic2 = undenorm (2.0f * v2 - ic2);
        const float band = v1 * k;
        const float low = v2;
        // a real wah is mostly a resonant bandpass with some low end left in
        x[i] = 2.2f * band + 0.25f * low;
    }
}

// The classic 60s wah: an inductor and a transistor. The peak sweeps a narrower, lower range
// (about 420 Hz to 1.8 kHz), is sharpest at the heel where an inductor's Q is highest, leaves
// the deep bass out, and the little transistor stage rounds off the loudest peaks.
void Wah::processClassic (float* x, int n)
{
    const float hpCoef = (float) std::exp (-2.0 * kPi * 90.0 / sr);
    for (int i = 0; i < n; ++i)
    {
        posSmooth.setTarget (pos);
        const float p = posSmooth.next();
        const float f = expMap (p, 420.0f, 1800.0f);
        const float q = lerp (lerp (5.0f, 11.0f, res), lerp (3.2f, 7.0f, res), p);
        const float k = 1.0f / q;
        const float g = (float) std::tan (kPi * f / sr);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        // the input coupling keeps the lowest notes from booming through the peak
        hp = undenorm (x[i] + hpCoef * (hp - x[i])); // hp follows the lows; what is left is the signal
        const float in = x[i] - hp;
        const float v3 = in - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = undenorm (2.0f * v1 - ic1);
        ic2 = undenorm (2.0f * v2 - ic2);
        const float band = v1 * k;
        const float y = 3.0f * band + 0.1f * v2;
        x[i] = std::tanh (1.3f * y) / 1.3f;
    }
}

//==============================================================================
// PitchPedal
void PitchPedal::prepare (double sampleRate)
{
    sr = sampleRate;
    trackLp.setCutoff (sr, 500.0);
    subLp1.setCutoff (sr, 300.0);
    subLp2.setCutoff (sr, 300.0);
    upHp.setCutoff (sr, 120.0);
    subEnv.prepare (sr, 3.0f, 60.0f);
    window = (int) (0.022 * sr); // 22 ms grains: about 11 ms of shifter latency
    line.prepare (window * 2 + 8);
    mixS.prepare (sr, 30.0f);
    reset();
}

void PitchPedal::reset()
{
    trackLp.reset(); subLp1.reset(); subLp2.reset(); upHp.reset(); subEnv.reset();
    flip = 1.0f; lastSign = 0.0f;
    line.reset();
    phase = 0.0f;
    mixS.reset (mix);
}

void PitchPedal::set (int m, float semitones, float mixKnob)
{
    mode = m;
    ratio = std::pow (2.0f, semitones / 12.0f);
    mix = clamp01 (mixKnob / 10.0f);
    mixS.setTarget (mix);
}

void PitchPedal::process (float* x, int n)
{
    float m = mixS.get();
    float dryG = mode == 2 ? std::cos (m * 1.5707963f) : 1.0f;
    float wetG = mode == 2 ? std::sin (m * 1.5707963f) : m * 1.6f;
    for (int i = 0; i < n; ++i)
    {
        if (mixS.isSmoothing())
        {
            m = mixS.next();
            dryG = mode == 2 ? std::cos (m * 1.5707963f) : 1.0f;
            wetG = mode == 2 ? std::sin (m * 1.5707963f) : m * 1.6f;
        }
        const float in = x[i];
        float wet = 0.0f;
        if (mode == 0)
        {
            // flip-flop divider on the lowpassed signal, like analog octave pedals
            const float t = trackLp.lowpass (in);
            const float sign = t > 0.0f ? 1.0f : -1.0f;
            if (sign > 0.0f && lastSign < 0.0f) flip = -flip;
            lastSign = sign;
            const float e = subEnv.process (t);
            wet = subLp2.lowpass (subLp1.lowpass (flip * e * 1.4f));
        }
        else if (mode == 1)
        {
            // full-wave rectification doubles the frequency
            wet = upHp.highpass (std::abs (in)) * 1.5f;
        }
        else
        {
            // two crossfaded read heads sweeping through a short buffer
            line.push (in);
            phase += (1.0f - ratio) / (float) window;
            phase -= std::floor (phase);
            const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
            const float d1 = 1.0f + phase * (float) window;
            const float d2 = 1.0f + p2 * (float) window;
            const float w1 = std::sin (phase * (float) kPi);
            const float w2 = std::sin (p2 * (float) kPi);
            wet = (line.read (d1) * w1 * w1 + line.read (d2) * w2 * w2);
        }
        x[i] = in * dryG + wet * wetG;
    }
}

//==============================================================================
// DrivePedal
static constexpr int kNewtonSteps = 8;

void DrivePedal::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    // Every pedal is its circuit (Circuits.h). 8x below 64 kHz: the diodes and transistors make
    // harmonics far above the guitar, and at 4x they fold back as a digital grit the real pedals
    // never had (the pedal lab measures it). The rodent, the fuzz and the metal box clip hard
    // enough to need 16x: at 8x their fold-back sat 40 to 60 dB down, at 16x 61 to 75 dB.
    const int st = sampleRate < 64000.0 ? 3 : 2;
    os = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) st, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock);
    osHigh = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) st + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    osHigh->initProcessing ((size_t) maxBlock);
    osr = sr * (double) (1 << st);
    green.prepare (osr, kNewtonSteps);
    rodent.prepare (2.0 * osr, kNewtonSteps);
    fuzz.prepare (2.0 * osr, kNewtonSteps);
    boost.prepare (osr, kNewtonSteps);
    trebleBoost.prepare (osr, kNewtonSteps);
    blues.prepare (osr, kNewtonSteps);
    metal.prepare (2.0 * osr, kNewtonSteps);
    levelS.prepare (osr, 30.0f);
    dirty = true;
    update();
    levelS.reset (levelS.getTarget());
    reset();
}

void DrivePedal::reset()
{
    if (os) os->reset();
    if (osHigh) osHigh->reset();
    green.reset(); rodent.reset(); fuzz.reset();
    boost.reset(); trebleBoost.reset(); blues.reset(); metal.reset();
    setPots (drive, tone, true); // a fresh start is already where the knobs are
    // A circuit starts with every capacitor empty and charges to where its bias holds it, and
    // that charging comes out as a thump (-49 dB from the blues drive and the metal box; the
    // others start where they rest). Run those on silence for 10 ms first, which leaves under
    // -84 dB, as a real pedal is already powered when it is stepped on. Costs 0.3 to 0.7 ms.
    if (type == 3 || type == 6)
    {
        const double rate = active() == osHigh.get() ? 2.0 * osr : osr;
        for (int i = 0, n = (int) (0.010 * rate); i < n; ++i)
            runCircuit (0.0);
    }
}

float DrivePedal::latencySamples() const { return active() != nullptr ? active()->getLatencyInSamples() : 0.0f; }

void DrivePedal::set (int t, float d, float tn, float lv)
{
    if (t != type || d != drive * 10.0f || tn != tone * 10.0f || lv != level * 10.0f)
    {
        const bool typeChanged = t != type;
        type = t; drive = clamp01 (d / 10.0f); tone = clamp01 (tn / 10.0f); level = clamp01 (lv / 10.0f);
        if (typeChanged) driveNow = -1.0; // another pedal: its pots start where the knobs are
        dirty = true;
        update();
        if (typeChanged) reset();
    }
}

void DrivePedal::update()
{
    if (! dirty) return;
    dirty = false;
    // the first setting, or a new pedal type, lands at once; a knob on the same pedal glides
    if (driveNow < 0.0) setPots (drive, tone, true);
    // each circuit sets its own level, as the real pedal's volume at noon; the level knob
    // gives 15 dB either side of that
    levelS.setTarget (dbToGain ((level - 0.5f) * 30.0f));
}

void DrivePedal::setPots (double d, double t, bool all)
{
    driveNow = d;
    toneNow = t;
    if (all || type == 0) boost.set (d, t);
    if (all || type == 1) trebleBoost.set (d, t);
    if (all || type == 2) green.set (d, t);
    if (all || type == 3) blues.set (d, t);
    if (all || type == 4) rodent.set (d, t);
    if (all || type == 5) fuzz.set (d, t);
    if (all || type == 6) metal.set (d, t);
}

double DrivePedal::runCircuit (double v) noexcept
{
    switch (type)
    {
        case 0: return boost.process (v);
        case 1: return trebleBoost.process (v);
        case 2: return green.process (v);
        case 3: return blues.process (v);
        case 4: return rodent.process (v);
        case 5: return fuzz.process (v);
        default: return metal.process (v);
    }
}

void DrivePedal::process (float* data, int n)
{
    auto* over = active();
    if (over == nullptr) return;
    float* chans[] = { data };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    auto up = over->processSamplesUp (block);
    float* x = up.getChannelPointer (0);
    const int on = (int) up.getNumSamples();
    constexpr double toVolts = circuit::kVoltsPerUnit, fromVolts = 1.0 / circuit::kVoltsPerUnit;
    const double rate = over == osHigh.get() ? 2.0 * osr : osr;
    const double k = 1.0 - std::exp (-32.0 / (0.020 * rate)); // per 32 oversampled samples
    for (int i = 0; i < on; ++i)
    {
        if ((i & 31) == 0 && (driveNow != (double) drive || toneNow != (double) tone))
        {
            double d = driveNow + ((double) drive - driveNow) * k, t = toneNow + ((double) tone - toneNow) * k;
            if (std::abs (d - drive) < 1.0e-4) d = drive;
            if (std::abs (t - tone) < 1.0e-4) t = tone;
            setPots (d, t, false);
        }
        x[i] = (float) (runCircuit ((double) x[i] * toVolts) * fromVolts) * levelS.next();
    }
    over->processSamplesDown (block);
}

//==============================================================================
// GraphicEq
void GraphicEq::prepare (double sampleRate)
{
    sr = sampleRate;
    glide = (float) std::exp (-16.0 / (0.020 * sr));
    levelS.prepare (sr, 20.0f);
    const float z[7] {};
    initialised = false;
    set (z, 0.0f);
    reset();
}

void GraphicEq::reset()
{
    for (auto& b : bands) b.reset();
    // a fresh start lands on the sliders: nothing to glide from
    snap = true;
    for (int b = 0; b < 7; ++b) if (gainsNow[b] != gains[b]) setBand (b, gains[b]);
    levelS.reset (level);
}

void GraphicEq::setBand (int b, float db)
{
    static const double freqs[7] = { 100, 200, 400, 800, 1600, 3200, 6400 };
    // a band that was out of the path comes in from silence, not from what it held long ago
    if (gainsNow[b] == 0.0f && db != 0.0f) bands[b].reset();
    gainsNow[b] = db;
    bands[b].set (Biquad::Type::peak, sr, freqs[b], 1.4, db);
}

void GraphicEq::set (const float* g, float levelDb)
{
    for (int i = 0; i < 7; ++i)
        if (g[i] != gains[i] || ! initialised)
        {
            gains[i] = g[i];
            if (snap || ! initialised) setBand (i, g[i]);
        }
    initialised = true;
    level = dbToGain (levelDb);
    levelS.setTarget (level);
}

void GraphicEq::process (float* x, int n)
{
    snap = false;
    for (int i = 0; i < n;)
    {
        for (int b = 0; b < 7; ++b)
            if (gainsNow[b] != gains[b])
                setBand (b, std::abs (gainsNow[b] - gains[b]) < 0.01f ? gains[b] : gains[b] + (gainsNow[b] - gains[b]) * glide);
        const int end = std::min (n, i + 16);
        for (; i < end; ++i)
        {
            float s = x[i];
            for (int b = 0; b < 7; ++b)
                if (gainsNow[b] != 0.0f) s = bands[b].process (s);
            x[i] = s * levelS.next();
        }
    }
}

//==============================================================================
// Modulation
void Modulation::prepare (double sampleRate)
{
    sr = sampleRate;
    lfo.prepare (sr);
    dl.prepare ((int) (0.05 * sr));
    dr.prepare ((int) (0.05 * sr));
    depthS.prepare (sr, 40.0f);
    mixS.prepare (sr, 40.0f);
    reset();
}

void Modulation::reset()
{
    dl.reset(); dr.reset();
    fbL = fbR = 0.0f;
    std::fill (std::begin (apL), std::end (apL), 0.0f);
    std::fill (std::begin (apR), std::end (apR), 0.0f);
}

void Modulation::listen (const float* l, const float* r, int n)
{
    for (int i = 0; i < n; ++i) { dl.push (l[i]); dr.push (r[i]); }
}

void Modulation::wake()
{
    fbL = fbR = 0.0f;
    std::fill (std::begin (apL), std::end (apL), 0.0f);
    std::fill (std::begin (apR), std::end (apR), 0.0f);
}

void Modulation::set (int t, float rate, float d, float m)
{
    // a new type starts with nothing ringing but the delay lines still full, so a chorus
    // picked while playing has its voices at once (the rig crossfades from the old type)
    if (t != type) { type = t; wake(); }
    // vibe and tremolo like faster top speeds
    rateHz = expMap (rate / 10.0f, 0.05f, type == 3 ? 14.0f : 10.0f);
    lfo.setRate (rateHz);
    depthS.setTarget (clamp01 (d / 10.0f));
    mixS.setTarget (clamp01 (m / 10.0f));
}

void Modulation::process (float* l, float* r, int n)
{
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float d = depthS.next(), m = mixS.next();
        const float inL = l[i], inR = r[i];
        float outL = inL, outR = inR;

        switch (type)
        {
            case 0: // chorus: two modulated voices in quadrature
            case 4: // vibrato: the same, fully wet
            {
                dl.push (inL); dr.push (inR);
                const float base = type == 0 ? 8.0f : 5.0f;
                const float sweep = (type == 0 ? 4.0f : 3.0f) * d;
                const float wl = dl.read ((base + sweep * (0.5f + 0.5f * lfo.sine (0.0f))) * msToS);
                const float wr = dr.read ((base + sweep * (0.5f + 0.5f * lfo.sine (0.25f))) * msToS);
                if (type == 0)
                {
                    const float wet = m * 0.8f;
                    outL = inL * (1.0f - 0.35f * m) + wl * wet;
                    outR = inR * (1.0f - 0.35f * m) + wr * wet;
                }
                else
                {
                    outL = lerp (inL, wl, 0.3f + 0.7f * m);
                    outR = lerp (inR, wr, 0.3f + 0.7f * m);
                }
                break;
            }
            case 1: // flanger, mix knob sets the feedback
            {
                const float fb = m * 0.85f;
                dl.push (inL + fbL * fb);
                dr.push (inR + fbR * fb);
                const float tri = lfo.triangle (0.0f), tri2 = lfo.triangle (0.25f);
                fbL = dl.read ((0.4f + d * 5.0f * (0.5f + 0.5f * tri)) * msToS);
                fbR = dr.read ((0.4f + d * 5.0f * (0.5f + 0.5f * tri2)) * msToS);
                fbL = fastTanh (fbL); fbR = fastTanh (fbR);
                outL = 0.6f * inL + 0.6f * fbL;
                outR = 0.6f * inR + 0.6f * fbR;
                break;
            }
            case 2: // phaser: six first-order allpass stages
            case 5: // vibe: four staggered stages with a lopsided sweep
            {
                const int stagesN = type == 2 ? 6 : 4;
                auto run = [&] (float in, float* st, float phaseOff, float& fbState)
                {
                    float lv = lfo.sine (phaseOff);
                    if (type == 5) lv = lv > 0 ? std::sqrt (lv) : -lv * lv; // photocell lag shape
                    const float f = expMap (0.5f + 0.5f * lv * d, 180.0f, 3200.0f);
                    float s = in + fbState * (type == 2 ? m * 0.7f : 0.2f);
                    for (int k = 0; k < stagesN; ++k)
                    {
                        const float fk = type == 5 ? f * (0.6f + 0.35f * (float) k) : f;
                        const float t = (float) std::tan (kPi * std::min (fk, 0.45f * (float) sr) / sr);
                        const float a = (t - 1.0f) / (t + 1.0f);
                        const float y = a * s + st[k];
                        st[k] = undenorm (s - a * y);
                        s = y;
                    }
                    fbState = s;
                    return s;
                };
                const float pl = run (inL, apL, 0.0f, fbL);
                const float pr = run (inR, apR, type == 2 ? 0.2f : 0.0f, fbR);
                const float wet = type == 2 ? 0.5f : (0.35f + 0.3f * m);
                outL = inL * (1.0f - wet) + pl * wet;
                outR = inR * (1.0f - wet) + pr * wet;
                break;
            }
            case 3: // tremolo, mix knob morphs sine into square
            {
                auto shape = [m] (float s)
                {
                    const float k = 1.0f + m * 12.0f;
                    return fastTanh (s * k) / fastTanh (k);
                };
                const float gl = 1.0f - d * (0.5f + 0.5f * shape (lfo.sine (0.0f)));
                const float gr = 1.0f - d * (0.5f + 0.5f * shape (lfo.sine (m > 0.5f ? 0.5f : 0.0f)));
                outL = inL * gl * (1.0f + 0.5f * d);
                outR = inR * gr * (1.0f + 0.5f * d);
                break;
            }
            default: break;
        }
        lfo.advance();
        l[i] = outL;
        r[i] = outR;
    }
}

//==============================================================================
// Delay
void Delay::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.prepare ((int) (2.2 * sr));
    dr.prepare ((int) (2.2 * sr));
    timeS.prepare (sr, 120.0f);
    timeS.reset ((float) (0.38 * sr));
    xfStep = (float) (1.0 / (0.050 * sr));
    mixS.prepare (sr, 30.0f);
    wow.prepare (sr); wow.setRate (0.55f);
    flutter.prepare (sr); flutter.setRate (6.3f);
    hpL.setCutoff (sr, 120.0); hpR.setCutoff (sr, 120.0);
    reset();
}

void Delay::reset()
{
    dl.reset(); dr.reset();
    lpL.reset(); lpR.reset(); hpL.reset(); hpR.reset();
    headA = -1.0f; crossing = false; // the next block starts at the time that is set
}

void Delay::set (int t, float timeMs, float fbKnob, float toneKnob, float mixKnob)
{
    type = t;
    timeS.setTarget ((float) (std::clamp (timeMs, 10.0f, 2000.0f) * 0.001 * sr));
    feedback = std::min (0.95f, (fbKnob / 10.0f) * 0.95f);
    mixS.setTarget (clamp01 (mixKnob / 10.0f));
    toneHz = expMap (toneKnob / 10.0f, 1200.0f, 16000.0f);
    if (type == 1) toneHz = std::min (toneHz, 3200.0f); // bucket-brigade bandwidth
    if (type == 2) toneHz = std::min (toneHz, 6500.0f);
    lpL.setCutoff (sr, toneHz);
    lpR.setCutoff (sr, toneHz);
}

void Delay::process (float* l, float* r, int n)
{
    for (int i = 0; i < n; ++i)
    {
        float t = timeS.next();
        float wl, wr;
        if (type == 0 || type == 3)
        {
            const float goal = timeS.getTarget();
            if (headA < 0.0f) headA = goal;
            if (! crossing && std::abs (goal - headA) > 0.5f) { headB = goal; xf = 0.0f; crossing = true; }
            wl = dl.read (headA); wr = dr.read (headA);
            if (crossing)
            {
                xf = std::min (1.0f, xf + xfStep);
                const float g = xf * xf * (3.0f - 2.0f * xf);
                wl += (dl.read (headB) - wl) * g;
                wr += (dr.read (headB) - wr) * g;
                if (xf >= 1.0f) { headA = headB; crossing = false; }
            }
        }
        else
        {
            if (type == 1) t *= 1.0f + 0.0015f * wow.sine();
            if (type == 2) t *= 1.0f + 0.0025f * wow.sine() + 0.0008f * flutter.sine();
            wl = dl.read (t); wr = dr.read (t);
            headA = -1.0f; crossing = false;
        }
        wow.advance(); flutter.advance();

        // keep the repeats musical and bounded
        auto loop = [this] (float s, OnePole& lp, OnePole& hp)
        {
            s = lp.lowpass (s);
            if (type >= 1) s = hp.highpass (s);
            if (type == 1 || type == 2) s = fastTanh (s * 1.2f) / 1.2f;
            return s;
        };
        wl = loop (wl, lpL, hpL);
        wr = loop (wr, lpR, hpR);

        const float mono = 0.5f * (l[i] + r[i]);
        if (type == 3)
        {
            // ping-pong: input enters left, repeats cross over
            dl.push (undenorm (mono + wr * feedback));
            dr.push (undenorm (wl * feedback));
        }
        else
        {
            dl.push (undenorm (l[i] + wl * feedback));
            dr.push (undenorm (r[i] + wr * feedback));
        }
        const float m = mixS.next();
        l[i] += wl * m;
        r[i] += wr * m;
    }
}

//==============================================================================
// Reverb: 8-line feedback delay network with input diffusion.
void Reverb::prepare (double sampleRate)
{
    sr = sampleRate;
    pre.prepare ((int) (0.25 * sr));
    for (auto& d : diff) d.prepare ((int) (0.05 * sr));
    for (auto& l : lines) l.prepare ((int) (0.4 * sr));
    springLine.prepare ((int) (0.1 * sr));
    shimmerLine.prepare ((int) (0.2 * sr));
    mod.prepare (sr);
    mod.setRate (0.31f);
    mixS.prepare (sr, 30.0f);
    glideK = 1.0f - (float) std::exp (-1.0 / (0.040 * sr));
    dirty = true;
    configure();
    std::copy (std::begin (lineGain), std::end (lineGain), std::begin (gainNow));
    preNow = -1.0f;
    reset();
}

void Reverb::reset()
{
    pre.reset();
    for (auto& d : diff) d.reset();
    for (auto& l : lines) l.reset();
    for (auto& d : damp) d.reset();
    for (auto& d : lowCut) d.reset();
    springLine.reset(); shimmerLine.reset();
    std::fill (std::begin (springAp), std::end (springAp), 0.0f);
    std::fill (std::begin (lastOut), std::end (lastOut), 0.0f);
    bandwidth.reset(); springLp.reset(); springHp.reset();
    shimPhase = 0.0f; // the shimmer's octave shifter starts each time from the same place
    // a fresh start has nothing to glide from
    std::copy (std::begin (lineGain), std::end (lineGain), std::begin (gainNow));
    preNow = -1.0f;
}

void Reverb::set (int t, float d, float p, float tn, float m)
{
    const float dd = clamp01 (d / 10.0f), tt = clamp01 (tn / 10.0f);
    if (t != type || dd != decay || p != preMs || tt != tone)
    {
        const bool fresh = t != type;
        if (fresh) { type = t; reset(); }
        decay = dd; preMs = p; tone = tt;
        dirty = true;
        configure();
        if (fresh) std::copy (std::begin (lineGain), std::end (lineGain), std::begin (gainNow));
    }
    mixS.setTarget (clamp01 (m / 10.0f));
}

void Reverb::configure()
{
    if (! dirty) return;
    dirty = false;
    float rt60 = 1.0f;
    switch (type)
    {
        case 0: size = 0.32f; rt60 = lerp (0.25f, 1.6f, decay); diffusion = 0.6f; modDepth = 0.1f; inputLp = 9000; break;
        case 1: size = 1.0f;  rt60 = lerp (1.0f, 7.0f, decay);  diffusion = 0.7f; modDepth = 0.35f; inputLp = 8000; break;
        case 2: size = 0.55f; rt60 = lerp (0.6f, 5.0f, decay);  diffusion = 0.8f; modDepth = 0.15f; inputLp = 14000; break;
        case 3: size = 0.3f;  rt60 = lerp (0.8f, 3.5f, decay);  diffusion = 0.65f; modDepth = 0.2f; inputLp = 5000; break;
        case 4: size = 1.1f;  rt60 = lerp (2.0f, 10.0f, decay); diffusion = 0.75f; modDepth = 0.5f; inputLp = 9000; break;
        default: size = 1.6f; rt60 = lerp (4.0f, 25.0f, decay); diffusion = 0.8f; modDepth = 0.9f; inputLp = 7000; break;
    }
    static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 73.1f };
    static const float diffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
    for (int i = 0; i < N; ++i)
    {
        lineLen[i] = baseMs[i] * size * 0.001f * (float) sr;
        lineGain[i] = std::pow (10.0f, -3.0f * (lineLen[i] / (float) sr) / rt60);
        // high frequencies die faster: tone sets the damping cutoff
        damp[i].setCutoff (sr, expMap (tone, 1500.0f, type == 2 ? 16000.0f : 11000.0f));
        lowCut[i].setCutoff (sr, type == 3 ? 250.0 : 60.0);
    }
    for (int i = 0; i < 4; ++i)
        diffLen[i] = diffMs[i] * std::max (0.4f, size) * 0.001f * (float) sr;
    bandwidth.setCutoff (sr, inputLp);
    springLp.setCutoff (sr, 4200.0);
    springHp.setCutoff (sr, 300.0);
    shimFb = type == 4 ? lerp (0.25f, 0.45f, decay) : 0.0f;
}

void Reverb::process (float* l, float* r, int n)
{
    // A turned Pre-delay knob slides the read point there (a jump would click); Decay glides too.
    const float preSamples = std::max (1.0f, preMs * 0.001f * (float) sr);
    if (preNow < 0.0f) preNow = preSamples;
    const float modAmt = modDepth * 0.0008f * (float) sr;
    const int shimWin = (int) (0.08 * sr);

    for (int i = 0; i < n; ++i)
    {
        float in = 0.5f * (l[i] + r[i]);
        pre.push (in);
        preNow += (preSamples - preNow) * glideK;
        float s = bandwidth.lowpass (pre.read (preNow));

        if (type == 3)
        {
            // spring tank: a dispersive allpass chain inside a short feedback loop
            // gives the classic "drip" and boing
            float sp = s + springLine.read (0.033f * (float) sr) * 0.55f;
            for (int k = 0; k < 12; ++k)
            {
                const float a = 0.62f;
                const float y = -a * sp + springAp[k];
                springAp[k] = undenorm (sp + a * y);
                sp = y;
            }
            sp = springHp.highpass (springLp.lowpass (sp));
            springLine.push (undenorm (sp));
            s = s * 0.3f + sp;
        }

        if (type == 4)
        {
            // shimmer: an octave-up copy of the tail is fed back into the input
            float tail = 0.0f;
            for (int k = 0; k < N; ++k) tail += lastOut[k];
            shimmerLine.push (tail * 0.25f);
            shimPhase += (1.0f - 2.0f) / (float) shimWin;
            shimPhase -= std::floor (shimPhase);
            const float p2 = shimPhase + 0.5f - std::floor (shimPhase + 0.5f);
            const float w1 = std::sin (shimPhase * (float) kPi), w2 = std::sin (p2 * (float) kPi);
            const float shifted = shimmerLine.read (1.0f + shimPhase * shimWin) * w1 * w1
                                  + shimmerLine.read (1.0f + p2 * shimWin) * w2 * w2;
            s += shifted * shimFb;
        }

        // input diffusion
        for (int k = 0; k < 4; ++k)
        {
            const float g = diffusion;
            const float delayed = diff[k].read (diffLen[k]);
            const float v = s + g * delayed;
            diff[k].push (undenorm (v));
            s = delayed - g * v;
        }

        // read the delay lines (two of them gently modulated)
        float y[N];
        for (int k = 0; k < N; ++k)
        {
            float len = lineLen[k];
            if (k < 4) len += modAmt * mod.sine (0.25f * (float) k) + modAmt;
            y[k] = lines[k].read (std::max (1.0f, len));
        }
        mod.advance();

        // Householder feedback matrix: y - 2/N * sum(y)
        float sum = 0.0f;
        for (int k = 0; k < N; ++k) sum += y[k];
        sum *= 2.0f / (float) N;
        for (int k = 0; k < N; ++k)
        {
            gainNow[k] += (lineGain[k] - gainNow[k]) * glideK;
            float fb = (y[k] - sum) * gainNow[k];
            fb = damp[k].lowpass (fb);
            fb = lowCut[k].highpass (fb);
            const float sign = (k & 1) ? -1.0f : 1.0f;
            lines[k].push (undenorm (fb + s * sign * 0.35f));
            lastOut[k] = y[k];
        }

        const float outL = (y[0] - y[2] + y[4] - y[6] + 0.5f * (y[1] + y[7])) * 0.55f;
        const float outR = (y[1] - y[3] + y[5] - y[7] + 0.5f * (y[2] + y[6])) * 0.55f;
        const float m = mixS.next();
        l[i] += outL * m;
        r[i] += outR * m;
    }
}

//==============================================================================
// LoFi
void LoFi::prepare (double sampleRate)
{
    sr = sampleRate;
    dl.prepare ((int) (0.05 * sr));
    dr.prepare ((int) (0.05 * sr));
    wow.prepare (sr); wow.setRate (0.7f);
    flutter.prepare (sr); flutter.setRate (5.5f);
    hissLp.setCutoff (sr, 6000.0);
    crackleLp.setCutoff (sr, 3000.0);
    noise = Noise { 0xBEEF };
    drift = 0.0f;
    hissLp.reset(); crackleLp.reset();
    wobS.prepare (sr, 30.0f);
    radioS.prepare (sr, 20.0f);
    mixS.prepare (sr, 20.0f);
    bitsS.prepare (sr, 30.0f);
    reset();
}

void LoFi::setRadio (float rd)
{
    radioNow = rd;
    const float hp = expMap (rd, 20.0f, 700.0f), lp = expMap (1.0f - rd, 2500.0f, 20000.0f);
    radioHpL.set (Biquad::Type::highpass, sr, hp, 0.8); radioHpR.set (Biquad::Type::highpass, sr, hp, 0.8);
    radioLpL.set (Biquad::Type::lowpass, sr, lp, 0.8);  radioLpR.set (Biquad::Type::lowpass, sr, lp, 0.8);
}

void LoFi::reset()
{
    dl.reset(); dr.reset();
    radioHpL.reset(); radioHpR.reset(); radioLpL.reset(); radioLpR.reset(); preLpL.reset(); preLpR.reset();
    holdCounter = 0; heldL = heldR = 0;
    wobS.reset (wobble); mixS.reset (mix); bitsS.reset (bits);
    if (radioNow >= 0.0f) { radioS.reset (radioTarget); setRadio (radioTarget); }
}

void LoFi::listen (const float* l, const float* r, int n)
{
    for (int i = 0; i < n; ++i) { dl.push (l[i]); dr.push (r[i]); }
}

void LoFi::wake()
{
    radioHpL.reset(); radioHpR.reset(); radioLpL.reset(); radioLpR.reset(); preLpL.reset(); preLpR.reset();
    holdCounter = 0; heldL = heldR = 0;
    // switched on, the knobs are where they are
    wobS.reset (wobble); mixS.reset (mix); bitsS.reset (bits);
    if (radioNow >= 0.0f) { radioS.reset (radioTarget); setRadio (radioTarget); }
}

void LoFi::set (float crush, float downsample, float wob, float nz, float radio, float m)
{
    bits = lerp (16.0f, 4.0f, clamp01 (crush / 10.0f));
    const float targetRate = expMap (1.0f - clamp01 (downsample / 10.0f), 2000.0f, (float) sr);
    holdRatio = std::max (1.0f, (float) sr / targetRate);
    preLpL.set (Biquad::Type::lowpass, sr, std::min (0.45 * targetRate, 0.45 * sr), 0.7);
    preLpR.set (Biquad::Type::lowpass, sr, std::min (0.45 * targetRate, 0.45 * sr), 0.7);
    wobble = clamp01 (wob / 10.0f);
    noiseAmt = clamp01 (nz / 10.0f);
    const float rd = clamp01 (radio / 10.0f);
    radioTarget = rd;
    if (radioNow < 0.0f) { radioS.reset (rd); setRadio (rd); }
    radioS.setTarget (rd);
    mix = clamp01 (m / 10.0f);
    wobS.setTarget (wobble);
    mixS.setTarget (mix);
    bitsS.setTarget (bits);
}

void LoFi::process (float* l, float* r, int n)
{
    float q = std::pow (2.0f, bitsS.get() - 1.0f);
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float inL = l[i], inR = r[i];
        float sl = inL, sr2 = inR;

        // the Radio knob's filters follow it in steps of 16 samples
        if ((i & 15) == 0)
        {
            const float rd = radioS.get();
            if (std::abs (rd - radioNow) > 1e-4f) setRadio (rd);
            q = std::pow (2.0f, bitsS.get() - 1.0f); // Crush glides through the bit depths too
        }
        radioS.next();
        const float bitsNow = bitsS.next();

        // tape wow and flutter with a slow random drift; the whole delay, its 6 ms base too,
        // grows with the knob, so turning it up from nothing slides in instead of jumping
        drift = 0.9995f * drift + 0.0005f * noise.next();
        const float w = wobS.next();
        const float wobL = w * (6.0f + 2.5f * wow.sine() + 0.35f * flutter.sine() + 30.0f * drift);
        const float wobR = w * (6.0f + 0.9f * (2.5f * wow.sine() + 0.35f * flutter.sine() + 30.0f * drift));
        wow.advance(); flutter.advance();
        dl.push (sl); dr.push (sr2);
        if (w > 1e-5f)
        {
            // three samples is the least the interpolation reads cleanly; under a twentieth of
            // the knob the plain signal is blended out, so nothing steps at either end
            const float blend = std::min (1.0f, w * 20.0f);
            sl = lerp (sl, dl.read (3.0f + std::max (0.0f, wobL) * msToS), blend);
            sr2 = lerp (sr2, dr.read (3.0f + std::max (0.0f, wobR) * msToS), blend);
        }

        // sample-rate reduction (sample and hold) and bit crushing
        if (holdRatio > 1.0f)
        {
            sl = preLpL.process (sl); sr2 = preLpR.process (sr2);
            holdCounter += 1.0f;
            if (holdCounter >= holdRatio) { holdCounter -= holdRatio; heldL = sl; heldR = sr2; }
            sl = heldL; sr2 = heldR;
        }
        if (bitsNow < 15.9f)
        {
            sl = std::round (sl * q) / q;
            sr2 = std::round (sr2 * q) / q;
        }

        sl = radioLpL.process (radioHpL.process (sl));
        sr2 = radioLpR.process (radioHpR.process (sr2));

        if (noiseAmt > 0.0f)
        {
            const float hiss = hissLp.lowpass (noise.next()) * 0.02f * noiseAmt;
            float crackle = 0.0f;
            if (std::abs (noise.next()) > 1.0f - 0.0004f * noiseAmt) crackle = noise.next() * 0.3f * noiseAmt;
            crackle = crackleLp.lowpass (crackle);
            sl += hiss + crackle;
            sr2 += hiss * 0.9f + crackle;
        }

        const float m = mixS.next();
        l[i] = lerp (inL, sl, m);
        r[i] = lerp (inR, sr2, m);
    }
}

//==============================================================================
// Doubler: a second, slightly late and drifting "performance" on the right.
void Doubler::prepare (double sampleRate)
{
    sr = sampleRate;
    line.prepare ((int) (0.06 * sr));
    lfo1.prepare (sr); lfo1.setRate (0.13f);
    lfo2.prepare (sr); lfo2.setRate (0.71f);
    tone.setCutoff (sr, 7000.0);
    amountS.prepare (sr, 50.0f);
    reset();
}

void Doubler::reset() { line.reset(); tone.reset(); }

void Doubler::set (float a) { amountS.setTarget (clamp01 (a / 10.0f)); }

void Doubler::process (float* l, float* r, int n)
{
    const float msToS = (float) (0.001 * sr);
    for (int i = 0; i < n; ++i)
    {
        const float a = amountS.next();
        const float mono = 0.5f * (l[i] + r[i]);
        line.push (mono);
        if (a <= 0.0001f) { lfo1.advance(); lfo2.advance(); continue; }
        const float d = (16.0f + 4.0f * lfo1.sine() + 0.6f * lfo2.sine()) * msToS;
        const float dbl = tone.lowpass (line.read (d));
        // equal-power spread: original leans left, double leans right
        l[i] = l[i] * (1.0f - 0.25f * a) + dbl * 0.15f * a;
        r[i] = r[i] * (1.0f - 0.85f * a) + dbl * 0.95f * a;
    }
}

//==============================================================================
// SafetyLimiter
void SafetyLimiter::prepare (double sampleRate)
{
    rel = (float) std::exp (-1.0 / (0.15 * sampleRate));
    reset();
}

void SafetyLimiter::reset() { env = 0.0f; }

void SafetyLimiter::process (float* l, float* r, int n)
{
    const float threshold = 0.5f; // -6 dBFS
    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max (std::abs (l[i]), std::abs (r[i]));
        env = peak > env ? peak : peak + (env - peak) * rel;
        const float g = env > threshold ? threshold / env : 1.0f;
        // gain riding catches sustained level, the soft clip catches the attack
        auto clip = [] (float x)
        {
            const float knee = 0.5f, ceiling = 0.966f;
            const float a = std::abs (x);
            if (a <= knee) return x;
            const float over = (a - knee) / (ceiling - knee);
            return std::copysign (knee + (ceiling - knee) * fastTanh (over), x);
        };
        const float gg = std::max (g, 0.25f);
        l[i] = clip (l[i] * (0.5f + 0.5f * gg));
        r[i] = clip (r[i] * (0.5f + 0.5f * gg));
    }
}
} // namespace rh
