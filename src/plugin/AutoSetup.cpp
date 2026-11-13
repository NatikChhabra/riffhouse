// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AutoSetup.h"

#include "../engine/DeviceNames.h"
#include "../engine/InputGuard.h"
#include "AudioSwitch.h"
#include "DeviceGuard.h"
#include "DeviceLog.h"

namespace rh
{
namespace
{
    constexpr float kHeardThreshold = 0.0032f; // -50 dBFS: something is playing
    constexpr float kSureThreshold = 0.063f;   // -24 dBFS: that is definitely a guitar
    constexpr float kMostLevelLossDb = 14.0f;  // a mode hearing the guitar this much quieter loses Windows' boost
    constexpr double kDwellMs = 1100.0;
    constexpr double kTuneMs = 1600.0;
    constexpr double kQuietMs = 2600.0;
    constexpr double kPauseMs = 1500.0;        // how long a finished step's result stays up
    constexpr double kPlayMs = 4500.0;         // hard playing wanted
    constexpr double kToneMs = 1800.0;         // of which this much to judge the treble, the rest to set the level
    constexpr double kPlayGiveUpMs = 20000.0;
    constexpr double kWaitForQuietMs = 9000.0; // then the noise is measured whatever is heard
    constexpr float kNoisyFixDb = -48.0f;      // hiss peaks over this with Input Fix on: the fix is held back
    constexpr float kMostSetupFixDb = 12.0f;   // the most Input Fix setup chooses by itself

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    bool isLoopback (const juce::String& name)
    {
        const auto n = name.toLowerCase();
        for (auto* w : { "stereo mix", "loopback", "what u hear", "wave out", "cable output", "voicemeeter", "monitor of" })
            if (n.contains (w)) return true;
        return false;
    }

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }

    juce::String defaultOutputFor (juce::AudioIODeviceType* t)
    {
        if (t == nullptr) return {};
        const auto names = t->getDeviceNames (false);
        const int idx = t->getDefaultDeviceIndex (false);
        return juce::isPositiveAndBelow (idx, names.size()) ? names[idx] : names[0];
    }
} // namespace

bool AutoSetup::isAsioWrapper (const juce::String& driver)
{
    const auto n = driver.toLowerCase();
    for (auto* w : { "asio4all", "flexasio", "generic low latency", "asio2wasapi", "realtek asio", "voicemeeter", "fl studio", "ableton" })
        if (n.contains (w)) return true;
    return false;
}

bool AutoSetup::looksLikeOnboardInput (const juce::String& inputName, const juce::String& typeName)
{
    if (typeName == "ASIO" && ! isAsioWrapper (inputName)) return false;
    const auto n = inputName.toLowerCase();
    // Audio interfaces and guitar cables have a proper high-impedance
    // instrument input, so they don't need the fix.
    for (auto* w : { "focusrite", "scarlett", "behringer", "umc", "audient", "presonus", "steinberg", "motu", "universal audio",
                     "volt", "apollo", "arturia", "minifuse", "m-audio", "m-track", "native instruments", "komplete audio",
                     "irig", "line 6", "helix", "pod ", "boss", "zoom", "tascam", "roland", "rubix", "yamaha", "esi ",
                     "solid state logic", "ssl ", "ik multimedia", "mackie", "nux", "valeton", "guitar", "instrument",
                     "hi-z", "usb audio codec", "rme", "antelope", "apogee", "id4", "id14", "evo",
                     "audiobox", "clarett", "saffire", "babyface", "fireface", "ur22", "ur12", "quad-capture", "duo-capture",
                     "katana", "hx stomp", "pod go", "kemper", "quad cortex", "ampero", "mooer", "hotone", "sonic port",
                     "alesis", "akai", "lexicon", "xenyx", "u-phoria", "uphoria", "soundcraft", "goxlr", "rodecaster",
                     "audio interface", "usb interface" })
        if (n.contains (w)) return false;
    return true;
}

AutoSetup::AutoSetup (juce::AudioDeviceManager& d, RiffhouseProcessor& p) : dm (d), proc (p) {}

AutoSetup::~AutoSetup()
{
    stopTimer();
    probe.cancel();
    proc.muteOutput.store (false);
}

void AutoSetup::setPhase (Phase p)
{
    currentPhase = p;
    phaseProgress = 0.0f;
    if (onPhaseChanged) onPhaseChanged();
}

void AutoSetup::pauseThen (const juce::String& found, std::function<void()> next)
{
    foundText = found;
    phaseProgress = 1.0f;
    afterPause = std::move (next);
    pauseUntil = nowMs() + kPauseMs;
    if (onPhaseChanged) onPhaseChanged();
}

std::vector<AutoSetup::Candidate> AutoSetup::findCandidates (juce::AudioDeviceManager& dm)
{
    std::vector<Candidate> candidates;
    // Windows Audio hears about plugging and unplugging by itself, and asking it again takes
    // long on a computer with many sound devices. The others are quick to ask.
    for (auto* t : dm.getAvailableDeviceTypes())
        if (! t->getTypeName().startsWith ("Windows Audio")) t->scanForDevices();

   #if JUCE_WINDOWS
    if (auto* asio = findType (dm, "ASIO"))
        for (auto& name : asio->getDeviceNames (true))
            if (! isAsioWrapper (name))
                candidates.push_back ({ "ASIO", name });
    if (auto* asio = findType (dm, "ASIO"))
    {
        // Realtek's own ASIO driver runs the computer's sockets with far less delay than Windows
        // does, but only when the guitar and the listening both use them: the sound would
        // otherwise go to sockets nobody listens on. So only when Windows plays there too.
        auto* wasapi = findType (dm, "Windows Audio");
        const auto outs = wasapi != nullptr ? wasapi->getDeviceNames (false) : juce::StringArray();
        const int def = wasapi != nullptr ? wasapi->getDefaultDeviceIndex (false) : -1;
        const bool listensOnRealtek = juce::isPositiveAndBelow (def, outs.size()) && outs[def].containsIgnoreCase ("realtek")
                                      && ! DeviceGuard::looksLikeScreen (outs[def]);
        bool guitarOnRealtek = false;
        if (wasapi != nullptr)
            for (const auto& in : wasapi->getDeviceNames (true))
                // a socket a cable goes into: not a laptop's built-in mic array or the "what you hear" loopback
                // (newer boards show their own chip as "Realtek USB Audio", so that counts too)
                guitarOnRealtek = guitarOnRealtek || (in.containsIgnoreCase ("realtek") && ! in.containsIgnoreCase ("stereo mix")
                                                      && ! in.containsIgnoreCase ("array"));
        for (auto& name : asio->getDeviceNames (true))
            if (name.containsIgnoreCase ("realtek asio") && listensOnRealtek && guitarOnRealtek)
                candidates.push_back ({ "ASIO", name });
    }
    if (auto* wasapi = findType (dm, "Windows Audio"))
    {
        const auto names = wasapi->getDeviceNames (true);
        const int def = wasapi->getDefaultDeviceIndex (true);
        if (juce::isPositiveAndBelow (def, names.size()) && ! isLoopback (names[def]))
            candidates.push_back ({ "Windows Audio", names[def] });
        for (int i = 0; i < names.size(); ++i)
            if (i != def && ! isLoopback (names[i]))
                candidates.push_back ({ "Windows Audio", names[i] });
    }
    {
        // Windows' own inputs in the order a guitar is most likely on them: one named for a
        // guitar or an instrument first (a cable often calls itself "Guitar"), then an audio
        // interface, then the rest as Windows lists them, and USB microphones and webcams last.
        auto rank = [] (const Candidate& c)
        {
            if (c.type == "ASIO") return 0;
            const auto n = c.name.toLowerCase();
            for (auto* w : { "guitar", "instrument", "hi-z", "hiz", "inst " })
                if (n.contains (w)) return 1;
            if (! looksLikeOnboardInput (c.name, c.type)) return 2;
            for (auto* w : { "fifine", "yeti", "webcam", "camera", "hyperx", "quadcast", "razer seiren", "headset", "airpods", "buds", "hands-free", "maono", "boya", "rode nt-usb", "samson" })
                if (n.contains (w)) return 4;
            return 3;
        };
        std::stable_sort (candidates.begin(), candidates.end(), [&] (const Candidate& x, const Candidate& y) { return rank (x) < rank (y); });
    }
    if (candidates.empty())
        if (auto* asio = findType (dm, "ASIO")) // only wrappers: still better than nothing
            for (auto& name : asio->getDeviceNames (true))
                candidates.push_back ({ "ASIO", name });
   #endif

    if (candidates.empty())
    {
        if (auto* t = dm.getCurrentDeviceTypeObject())
        {
            const auto names = t->getDeviceNames (true);
            const int def = t->getDefaultDeviceIndex (true);
            if (juce::isPositiveAndBelow (def, names.size()))
                candidates.push_back ({ t->getTypeName(), names[def] });
            for (int i = 0; i < names.size(); ++i)
                if (i != def && ! isLoopback (names[i]))
                    candidates.push_back ({ t->getTypeName(), names[i] });
        }
    }
    return candidates;
}

std::vector<AutoSetup::Input> AutoSetup::inputs (juce::AudioDeviceManager& dm) { return findCandidates (dm); }

void AutoSetup::buildCandidates()
{
    candidates = findCandidates (dm);
    candidatePeak.assign (candidates.size(), 0.0f);
}

void AutoSetup::start (const Input* only)
{
    stopTimer();
    probe.cancel(); // a test run from an earlier start must not steer this one
    original = AudioOpen::running (dm);
    devlog::note ("Setup started" + (only != nullptr ? " on " + only->type + " / " + only->name : juce::String (", finding the input")));
    result = {};
    best = {};
    afterPause = nullptr;
    foundText = {};
    playPeak = 0.0f;
    noiseStarted = fixRetried = false;
    pass = 0;
    candidateIndex = 0;
    proc.muteOutput.store (true);
    followOutput = {};
    buildCandidates();
    if (only != nullptr)
        for (const auto& c : candidates)
            if (c.type == only->type && c.name == only->name)
            {
                // the player said where the guitar is: listen there and nowhere else
                candidates = { c };
                candidatePeak.assign (1, 0.0f);
                break;
            }
    if (only == nullptr)
    {
        // a driver that failed or hung on this computer before is not tried again by itself
        std::vector<Candidate> keep;
        for (const auto& c : candidates)
            if (c.type != "ASIO" || ! AudioProbe::knownBad (c.name)) keep.push_back (c);
            else devlog::note ("Setup leaves out " + c.name + ": it failed on this computer before");
        candidates = keep;
        candidatePeak.assign (candidates.size(), 0.0f);
    }
    if (candidates.empty())
    {
        detailText = "No audio inputs found on this computer.";
        setPhase (Phase::noSignal);
        return;
    }
    setPhase (Phase::listening);
    checkNextDriver();
}

void AutoSetup::checkNextDriver()
{
    for (const auto& c : candidates)
    {
        if (c.type != "ASIO" || AudioProbe::provedThisRun (c.name)) continue;
        const auto name = c.name;
        detailText = "Checking " + name;
        if (onPhaseChanged) onPhaseChanged();
        dm.closeAudioDevice(); // the driver needs the sound devices free
        probe.start (AudioProbe::commandFor ("ASIO", name), AudioProbe::kTimeoutMs, [this, name] (AudioProbe::Result r)
        {
            devlog::note (AudioProbe::describe (r, name));
            if (r.outcome != AudioProbe::Outcome::couldNotStart) AudioProbe::remember (name, r.ok());
            if (! r.ok())
            {
                for (size_t i = 0; i < candidates.size(); ++i)
                    if (candidates[i].type == "ASIO" && candidates[i].name == name)
                    {
                        candidates.erase (candidates.begin() + (long) i);
                        candidatePeak.erase (candidatePeak.begin() + (long) i);
                        break;
                    }
                if (candidates.empty())
                {
                    detailText = name + " would not start, and there is no other input to try.";
                    if (original.valid()) openExactly (dm, original);
                    proc.muteOutput.store (false);
                    setPhase (Phase::noSignal);
                    return;
                }
            }
            checkNextDriver();
        });
        return;
    }
    // every driver left has played in a test run: listen
    if (candidates.empty())
    {
        detailText = "There is no input left to try.";
        if (original.valid()) openExactly (dm, original);
        proc.muteOutput.store (false);
        setPhase (Phase::noSignal);
        return;
    }
    openCandidate (candidates[0]);
    startTimerHz (25);
}

void AutoSetup::cancel()
{
    stopTimer();
    probe.cancel();
    afterPause = nullptr;
    if (currentPhase != Phase::finished && currentPhase != Phase::idle)
    {
        // what was running before setup comes back; when nothing was, nothing is left half open
        devlog::note ("Setup cancelled");
        if (original.valid()) openExactly (dm, original);
        else dm.closeAudioDevice();
    }
    proc.muteOutput.store (false);
    currentPhase = Phase::idle;
}

juce::String AutoSetup::outputFor (const Candidate& c, juce::AudioIODeviceType* t, const juce::String& current) const
{
    if (c.type == "ASIO") return c.name;
    const auto outputs = t != nullptr ? t->getDeviceNames (false) : juce::StringArray();
    if (followOutput.isNotEmpty() && outputs.contains (followOutput)) return followOutput;
    auto pick = current.isNotEmpty() && outputs.contains (current) ? current : defaultOutputFor (t);
    // Bluetooth and screens hold the sound back in every app. Wired headphones that Windows
    // sees plugged in ("Headphones") play in time, so they are chosen instead; a plain speaker
    // socket is not, since it may have nothing in it. The notice says why the rest lag.
    if (DeviceGuard::looksLikeBluetooth (pick) || DeviceGuard::looksLikeScreen (pick))
        for (const auto& o : outputs)
            if (o.containsIgnoreCase ("headphone") && ! DeviceGuard::looksLikeBluetooth (o) && ! DeviceGuard::looksLikeScreen (o)
                && ! DeviceGuard::looksLikeVirtual (o))
            {
                devlog::note ("Setup plays through " + o + " instead of " + pick + ", which adds delay");
                return o;
            }
    return pick;
}

bool AutoSetup::openCandidate (const Candidate& c)
{
    proc.tiltProbe().restart(); // a new input: measure its tone from scratch
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    const auto err = openExactly (dm, { c.type, c.name, outputFor (c, findType (dm, c.type), s.outputDeviceName), 0, 0.0 });
    detailText = "Listening on " + c.name;
    dwellStart = nowMs();
    dwellPeak = 0.0f;
    proc.rawInputPeak.store (0.0f);
    return err.isEmpty() && dm.getCurrentAudioDevice() != nullptr;
}

void AutoSetup::timerCallback()
{
    const double t = nowMs();
    const float raw = proc.rawInputPeak.exchange (0.0f);
    level = std::max (raw, level * 0.85f);
    const double sinceTick = lastTick > 0 ? std::min (200.0, t - lastTick) : 40.0;
    lastTick = t;

    if (afterPause != nullptr)
    {
        if (t < pauseUntil) return;
        auto next = std::move (afterPause);
        afterPause = nullptr;
        foundText = {};
        next();
        return;
    }

    switch (currentPhase)
    {
        case Phase::listening:
        {
            const double elapsed = t - dwellStart;
            if (elapsed > 200.0) dwellPeak = std::max (dwellPeak, raw);
            const int n = (int) candidates.size();
            if (n == 1)
            {
                phaseProgress = (float) std::min (1.0, elapsed / 15000.0);
                if (dwellPeak > kHeardThreshold && elapsed > 1200.0)
                {
                    chosen = candidates[0];
                    chosenPeak = dwellPeak;
                    pauseThen ("Found your guitar on " + chosen.name, [this] { beginTuning(); });
                }
                else if (elapsed > 15000.0)
                {
                    stopTimer();
                    proc.muteOutput.store (false);
                    setPhase (Phase::noSignal);
                }
                return;
            }
            phaseProgress = (float) std::min (1.0, ((double) (pass * n + candidateIndex) + elapsed / kDwellMs) / (double) (3 * n));
            if (dwellPeak > kSureThreshold && elapsed > 500.0)
            {
                chosen = candidates[(size_t) candidateIndex];
                chosenPeak = dwellPeak;
                pauseThen ("Found your guitar on " + chosen.name, [this] { beginTuning(); });
                return;
            }
            if (elapsed < kDwellMs) return;
            candidatePeak[(size_t) candidateIndex] = std::max (candidatePeak[(size_t) candidateIndex], dwellPeak);
            if (++candidateIndex >= n)
            {
                candidateIndex = 0;
                ++pass;
                const auto it = std::max_element (candidatePeak.begin(), candidatePeak.end());
                if (*it > kHeardThreshold)
                {
                    chosen = candidates[(size_t) (it - candidatePeak.begin())];
                    chosenPeak = *it;
                    pauseThen ("Found your guitar on " + chosen.name, [this] { beginTuning(); });
                    return;
                }
                if (pass >= 3)
                {
                    stopTimer();
                    openCandidate (candidates[0]);
                    proc.muteOutput.store (false);
                    setPhase (Phase::noSignal);
                    return;
                }
            }
            openCandidate (candidates[(size_t) candidateIndex]);
            return;
        }

        case Phase::tuning:
        {
            const double elapsed = t - dwellStart;
            if (elapsed > 300.0)
            {
                if (xrunStart < 0) xrunStart = dm.getXRunCount();
                if (modePeakFor != modeIndex) { modePeakFor = modeIndex; modePeak = 0.0f; }
                modePeak = std::max (modePeak, raw);
                tunePeak = std::max (tunePeak, raw);
                strumPeak = std::max (strumPeak, raw);
                cpuMax = std::max (cpuMax, dm.getCpuUsage());
            }
            phaseProgress = (float) std::min (0.98, ((double) modeIndex + (double) bufferIndex / 6.0 + elapsed / kTuneMs / 6.0) / (double) std::max (1, modes.size()));
            if (elapsed < kTuneMs) return;

            const int xr = dm.getXRunCount() - std::max (0, xrunStart);
            auto* dev = dm.getCurrentAudioDevice();
            const bool ok = ! lastOpenFailed && dev != nullptr && xr <= 0 && cpuMax < 0.7 && tunePeak > 1.0e-7f;
            bool settled = false;
            // Some inputs come in far quieter with the sound card to ourselves: Exclusive Mode skips
            // the level and boost Windows applies (a USB guitar input measured 20 dB down, with the
            // same hiss), and the boost that makes up for it turns the hiss into fizz. The guitar was
            // found in Windows' own mode, so a mode that hears it much quieter is not used.
            const float quieterDb = juce::Decibels::gainToDecibels (chosenPeak, -100.0f) - juce::Decibels::gainToDecibels (modePeak, -100.0f);
            const bool losesLevel = ok && chosen.type == "Windows Audio" && modes[modeIndex] != "Windows Audio"
                                    && chosenPeak > kHeardThreshold && quieterDb > kMostLevelLossDb;
            if (losesLevel)
            {
                devlog::note (modes[modeIndex] + " hears the guitar " + juce::String (quieterDb, 1)
                              + " dB quieter than Windows Audio: not used, its boost would only add hiss");
                result.quieterModeSkipped = true;
                ++modeIndex;
                bufferIndex = 0;
            }
            else if (ok)
            {
                const double lat = measuredLatencyMs();
                if (lat < best.latency)
                    best = { modes[modeIndex], dev->getCurrentBufferSizeSamples(), lat, dev->getCurrentSampleRate() };
                // A mode with the sound card to itself that plays clean is kept: shared mode, which
                // only adds Windows' own mixing delay, is never tried over it.
                settled = ! AudioSwitch::isShared (modes[modeIndex]);
                ++modeIndex; // this mode's best is found, try the next mode
                bufferIndex = 0;
            }
            else
                ++bufferIndex;

            if (settled || best.latency <= 12.0 || modeIndex >= modes.size())
            {
                finishTuning();
                return;
            }
            if (bufferIndex >= std::min (6, buffers.size()) || bufferIndex == 0)
            {
                if (bufferIndex != 0) { ++modeIndex; bufferIndex = 0; }
                // open the next mode and find its buffer sizes
                while (modeIndex < modes.size())
                {
                    if (openMode (modes[modeIndex], 0) && ! buffers.isEmpty())
                        break;
                    ++modeIndex;
                }
                if (modeIndex >= modes.size()) { finishTuning(); return; }
            }
            openMode (modes[modeIndex], buffers[bufferIndex]);
            return;
        }

        case Phase::playing:
        {
            const double elapsed = t - phaseStart;
            playPeak = std::max (playPeak, raw);
            // playing counts while it is loud: the ring-out between strums is not what the level is set by
            if (raw > std::max (2.0f * kHeardThreshold, 0.1f * playPeak)) playedMs += sinceTick;
            phaseProgress = (float) std::min (1.0, playedMs / kPlayMs);
            // First the treble: once that is judged Input Fix goes on, and the rest of the
            // playing is heard through it, because the fix lifts the peaks the level is set by.
            if (! fixSet && playedMs >= kToneMs)
            {
                setFixFromTone();
                fixSetAt = t;
            }
            if (fixSet)
            {
                const float stage = proc.meters().stagePeak.exchange (0.0f);
                if (t - fixSetAt > 250.0) fixedPeak = std::max (fixedPeak, stage); // once the fix has settled in
            }
            if (playedMs >= kPlayMs || elapsed > kPlayGiveUpMs) finishPlaying();
            return;
        }

        case Phase::quiet:
        {
            if (! noiseStarted)
            {
                // nothing is measured until the strings have really stopped
                const float still = std::max (2.0f * kHeardThreshold, 0.05f * std::max (playPeak, strumPeak));
                if (raw > still) quietSince = t;
                if (t - quietSince > 700.0 || t - phaseStart > kWaitForQuietMs) startNoise();
                return;
            }
            const double elapsed = t - noiseStart;
            phaseProgress = (float) std::min (1.0, elapsed / kQuietMs);
            if (elapsed > 300.0) noiseSamples.add (proc.meters().gateInPeak.load());
            if (elapsed >= kQuietMs) finish();
            return;
        }

        case Phase::idle:
        case Phase::finished:
        case Phase::noSignal:
            stopTimer();
            return;
    }
}

void AutoSetup::beginTuning()
{
    setPhase (Phase::tuning);
    strumPeak = chosenPeak;
    // An audio interface is listened through as well as played into: its own output comes with it.
    if (auto* t = findType (dm, chosen.type))
        followOutput = AudioSwitch::outputToFollow (chosen.type, chosen.name, t->getDeviceNames (false));
    modes.clear();
    if (chosen.type == "ASIO")
        modes.add ("ASIO");
    else if (chosen.type.startsWith ("Windows Audio"))
    {
        for (auto* m : { "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)", "Windows Audio" })
            if (findType (dm, m) != nullptr) modes.add (m);
    }
    else
        modes.add (chosen.type);

    modeIndex = 0;
    bufferIndex = 0;
    while (modeIndex < modes.size() && ! (openMode (modes[modeIndex], 0) && ! buffers.isEmpty()))
        ++modeIndex;
    if (modeIndex >= modes.size()) { finishTuning(); return; }
    openMode (modes[modeIndex], buffers[0]);
}

bool AutoSetup::openMode (const juce::String& type, int bufferSize)
{
    auto* t = findType (dm, type);
    if (t == nullptr) return false;
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    // 48 kHz when the device has it, otherwise the rate it already runs at
    AudioOpen o { type, chosen.name, outputFor ({ type, chosen.name }, t, s.outputDeviceName), bufferSize, 48000.0 };
    auto err = openExactly (dm, o);
    auto* dev = dm.getCurrentAudioDevice();
    if ((err.isNotEmpty() || dev == nullptr) && type == "ASIO")
    {
        // an interface held at another rate (by its own settings, or by Windows using it): keep its rate
        o.sampleRate = 0;
        err = openExactly (dm, o);
        dev = dm.getCurrentAudioDevice();
    }
    if ((err.isNotEmpty() || dev == nullptr) && bufferSize == 0 && followOutput.isNotEmpty() && type != "ASIO")
    {
        // the interface's output would not open alongside its input: stay on the output that was playing
        followOutput = {};
        o.output = defaultOutputFor (t);
        err = openExactly (dm, o);
        dev = dm.getCurrentAudioDevice();
    }
    lastOpenFailed = err.isNotEmpty() || dev == nullptr;
    // Windows runs the guitar's device and the headphones at different rates (192 and 48 kHz,
    // say): the low latency mode needs one rate for both, and the player can set that
    if (lastOpenFailed && type == "Windows Audio (Low Latency Mode)" && err.containsIgnoreCase ("sample rate"))
        result.ratesDiffer = true;
    if (lastOpenFailed)
    {
        dwellStart = nowMs() - kTuneMs; // judge it right away as a failure
        return false;
    }

    if (bufferSize == 0)
    {
        buffers = rh::buffersToTry (dev->getAvailableBufferSizes());
        if (buffers.isEmpty()) buffers.add (dev->getCurrentBufferSizeSamples());
    }

    juce::String modeName = type;
    if (type == "Windows Audio (Exclusive Mode)") modeName = "exclusive mode";
    else if (type == "Windows Audio (Low Latency Mode)") modeName = "low latency mode";
    else if (type == "Windows Audio") modeName = "shared mode";
    detailText = "Trying " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples in " + modeName;
    dwellStart = nowMs();
    xrunStart = -1;
    tunePeak = 0.0f;
    cpuMax = 0.0;
    return true;
}

double AutoSetup::measuredLatencyMs() const
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) return 1e9;
    const double sr = std::max (1.0, dev->getCurrentSampleRate());
    int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
    if (lat <= 0) lat = 2 * dev->getCurrentBufferSizeSamples();
    return 1000.0 * (lat + proc.getLatencySamples()) / sr;
}

void AutoSetup::finishTuning()
{
    auto* running = dm.getCurrentAudioDevice();
    const bool alreadyBest = running != nullptr && best.buffer > 0 && ! lastOpenFailed
                             && dm.getCurrentAudioDeviceType() == best.type && running->getCurrentBufferSizeSamples() == best.buffer;
    if (alreadyBest)
        devlog::note ("Setup keeps " + juce::String (best.buffer) + " samples, already open"); // a second open only stalls the window
    else if (best.buffer > 0)
        openMode (best.type, best.buffer);
    else
        openCandidate (chosen); // nothing was perfect: keep the safe default buffer

    if (auto* dev = dm.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        result.deviceType = dev->getTypeName();
        result.inputName = s.inputDeviceName;
        result.outputName = s.outputDeviceName;
        result.bufferSize = dev->getCurrentBufferSizeSamples();
        result.sampleRate = dev->getCurrentSampleRate();
        result.latencyMs = measuredLatencyMs();
        result.exclusive = result.deviceType.contains ("Exclusive");
        result.outputFollows = followOutput.isNotEmpty() && s.outputDeviceName == followOutput;
    }
    detailText = {};
    pauseThen ("About " + juce::String (result.latencyMs, 1) + " ms from string to ear", [this] { beginPlaying(); });
}

void AutoSetup::beginPlaying()
{
    proc.tiltProbe().restart(); // the tone is judged on this playing, on the device as it will run
    proc.rawInputPeak.store (0.0f);
    playPeak = fixedPeak = 0.0f;
    playedMs = 0;
    fixSet = false;
    proc.setValue (P::inGain, 0.0f); // the level into the pedals is read as it comes, then set
    phaseStart = nowMs();
    detailText = {};
    setPhase (Phase::playing);
}

void AutoSetup::setFixFromTone()
{
    // Input Fix: measure the treble that actually arrives when there was enough playing
    // to judge, otherwise go by what kind of input this is.
    const bool onboard = looksLikeOnboardInput (result.inputName, result.deviceType);
    result.toneSeconds = proc.tiltProbe().secondsMeasured();
    result.tiltDb = proc.tiltProbe().tiltDb();
    if (result.toneSeconds >= 1.0)
    {
        // A mic or line jack loads the pickup and every bit of lost treble is worth having
        // back. Through an interface a darker reading is mostly a dark guitar or pickup,
        // which is part of its sound: only a clear loss is mended there.
        result.inputFixDb = TiltProbe::fixAmountFor (result.tiltDb, onboard ? 3.0f : 5.0f);
        if (! onboard && result.inputFixDb < 9.0f) result.inputFixDb = 0.0f;
        // Setup gives at most 12 dB. A pickup into a PC jack loses its treble faster than the
        // fix's curve rises, and what is missing above 2.5 kHz sits close to the jack's hiss:
        // on a real take (-29 dB tilt, asking for the full 24) 24 dB came out harsh and hissy,
        // 12 dB measured like a recorded clean guitar. More is a turn of the knob away.
        result.inputFixDb = std::min (result.inputFixDb, kMostSetupFixDb);
    }
    else
        result.inputFixDb = onboard ? 12.0f : 0.0f;
    result.inputFix = result.inputFixDb > 0.0f;
    proc.setValue (P::inputFix, result.inputFix ? 1.0f : 0.0f);
    proc.setValue (P::inputFixAmt, result.inputFix ? result.inputFixDb : 6.0f);
    proc.setValue (P::humFilter, 1.0f); // Auto
    proc.meters().stagePeak.store (0.0f);
    fixSet = true;
}

void AutoSetup::finishPlaying()
{
    if (! fixSet) setFixFromTone();
    // The level is set from the hardest playing, and when the player did play for this
    // step, from this step alone: the first strums that got the input noticed are careful
    // ones, and a rig set by them runs far too hot once the playing is for real. The peak
    // that counts is the one after Input Fix, which is what reaches the pedals.
    result.playedSeconds = playedMs / 1000.0;
    float peak = std::max (playPeak, fixedPeak);
    if (playedMs < 1500.0) peak = std::max (peak, std::max (chosenPeak, strumPeak));
    result.guitarPeakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    result.clipping = playPeak > 0.97f;
    // aim the hardest peaks at about -12 dBFS into the rig
    float gain = juce::jlimit (-12.0f, 30.0f, InputGuard::kTargetDb - result.guitarPeakDb);
    gain = std::round (gain * 2.0f) * 0.5f;
    result.inputGainDb = gain;
    proc.setValue (P::inGain, gain);
    // from here the input guard may turn it down further, for as long as the player leaves it alone
    proc.uiState.setProperty ("guardGain", gain, nullptr);
    // these seconds of digging in are what every tone is levelled with from now on
    proc.levels().learnPhraseNow();

    pauseThen ("Input level set to " + juce::String (gain >= 0 ? "+" : "") + juce::String (gain, 1) + " dB", [this] { beginQuiet(); });
}

void AutoSetup::beginQuiet()
{
    noiseSamples.clearQuick();
    noiseStarted = false;
    fixRetried = false;
    phaseStart = quietSince = nowMs();
    detailText = {};
    setPhase (Phase::quiet);
}

void AutoSetup::startNoise()
{
    noiseStarted = true;
    noiseStart = nowMs();
    noiseSamples.clearQuick();
    if (onPhaseChanged) onPhaseChanged();
}

void AutoSetup::finish()
{
    // a high percentile ignores a single accidental bump
    std::vector<float> v (noiseSamples.begin(), noiseSamples.end());
    std::sort (v.begin(), v.end());
    const float noise = v.empty() ? 0.0f : v[(size_t) ((double) (v.size() - 1) * 0.9)];
    result.noiseDb = juce::Decibels::gainToDecibels (noise, -100.0f);

    // Input Fix brings the input's hiss up with the treble. When that leaves more hiss than
    // a gate can hide without cutting notes short, some of the fix is given back and the
    // noise is measured once more.
    if (result.inputFix && result.inputFixDb > 12.0f && result.noiseDb > kNoisyFixDb && ! fixRetried)
    {
        fixRetried = true;
        result.fixHeldBack = true;
        result.inputFixDb = std::max (12.0f, std::round ((result.inputFixDb - (result.noiseDb - kNoisyFixDb)) * 2.0f) * 0.5f);
        proc.setValue (P::inputFixAmt, result.inputFixDb);
        startNoise();
        return;
    }

    stopTimer();
    float gate = juce::jlimit (-90.0f, -42.0f, result.noiseDb + 6.0f);
    gate = std::round (gate);
    result.gateDb = gate;
    proc.setValue (P::gateThresh, gate);
    proc.rememberSetup(); // new copies of the plugin in a DAW start from this
    proc.muteOutput.store (false);
    devlog::note ("Setup finished: " + result.deviceType + " / " + result.inputName + " -> " + result.outputName + ", "
                  + juce::String (result.bufferSize) + " samples at " + juce::String (juce::roundToInt (result.sampleRate)) + " Hz, "
                  + juce::String (result.latencyMs, 1) + " ms, input " + juce::String (result.inputGainDb, 1) + " dB, fix "
                  + juce::String (result.inputFix ? result.inputFixDb : 0.0f, 1) + " dB, gate " + juce::String (juce::roundToInt (result.gateDb)) + " dB");
    setPhase (Phase::finished);
}
} // namespace rh
