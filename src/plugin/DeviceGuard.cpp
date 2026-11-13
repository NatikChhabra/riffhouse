// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceGuard.h"

#include "AudioOpen.h"
#include "DeviceLog.h"

namespace rh
{
namespace
{
    constexpr int kTickMs = 500;
    constexpr double kStallMs = 3000.0; // no audio callbacks for this long means the device has stopped
    // How long to wait before each go at a sound card that will not start. After the last one it
    // is left alone until the list of devices changes or the player asks.
    constexpr double kWaitMs[] = { 1500.0, 4000.0, 10000.0, 20000.0, 30000.0 };
    // Dropouts in the first moment after a device opens are not heard (the sound fades in), so
    // they are not counted. After that, this many within the window raise the buffer. One stray
    // dropout (a window being dragged, a virus scan starting) is let go.
    constexpr double kDropSettleMs = 1000.0;
    constexpr double kDropWindowMs = 10000.0;
    constexpr size_t kDropsToRaise = 3;
    constexpr int kMaxRaises = 6;      // a session never climbs further than this by itself
    constexpr int kLargestRaise = 2048; // past this the buffer is not what is wrong

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }
} // namespace

bool DeviceGuard::looksLikeBluetooth (const juce::String& name)
{
    const auto n = name.toLowerCase();
    for (auto* w : { "bluetooth", "airpods", "hands-free", "handsfree", "buds", "wh-1000", "wf-1000", "bose qc", "quietcomfort",
                     "jbl tune", "jbl live", "soundcore", "beats ", "powerbeats", "freebuds", "earbuds", "jabra", " a2dp", "bt audio",
                     // speakers and earphones sold mostly as Bluetooth, and the " Stereo" Windows adds to them
                     "rockerz", "airdopes", "boat ", "jbl flip", "jbl charge", "jbl go", "jbl clip", "srs-", "marshall", "tribit",
                     "zebronics", "mivi", "galaxy buds", "oneplus buds", "realme buds", "noise buds", " stereo)" })
    {
        // these two also name wired things: "Speakers (USB Audio Stereo)", a Marshall amp over USB
        const bool alsoWired = juce::String (w) == " stereo)" || juce::String (w) == "marshall";
        if (n.contains (w) && ! (alsoWired && (n.contains ("usb") || n.contains ("code")))) return true;
    }
    return false;
}

bool DeviceGuard::looksLikeScreen (const juce::String& name)
{
    // a TV or monitor over HDMI or DisplayPort: the screen holds the sound back to match its picture
    const auto n = name.toLowerCase();
    for (auto* w : { "hdmi", "displayport", "display audio", " tv ", " tv)", "- tv", "tv (", "nvidia high definition audio",
                     "amd high definition audio", "intel(r) display", "lg tv", "samsung tv", "samsung smart tv", "[tv] samsung", "sony tv", "mi tv", "bravia", "android tv" })
        if (n.contains (w)) return true;
    return false;
}

bool DeviceGuard::looksLikeVirtual (const juce::String& name)
{
    const auto n = name.toLowerCase();
    for (auto* w : { "voicemeeter", "vb-audio", "cable input", "cable output", "virtual", "steam streaming", "nvidia broadcast", "krisp", "obs" })
        if (n.contains (w)) return true;
    return false;
}

DeviceGuard::DeviceGuard (juce::AudioDeviceManager& d, std::atomic<juce::uint32>& b, std::atomic<bool>& m) : dm (d), beats (b), mute (m)
{
    dm.addChangeListener (this);
    clock = nowMs();
    lastMovedAt = clock;
    adopt();
    adoptSaved();
    knownInputs = allInputs();
    startTimer (kTickMs);
}

DeviceGuard::~DeviceGuard()
{
    stopTimer();
    dm.removeChangeListener (this);
    if (guardMuted) mute.store (false);
}

void DeviceGuard::setState (State s)
{
    if (s == current) return;
    const auto before = current;
    current = s;
    const char* names[] = { "no device", "healthy", "stalled", "lost" };
    devlog::note ("Connection: " + juce::String (names[(int) before]) + " -> " + names[(int) s]);
    if (s == State::healthy && (before == State::lost || before == State::stalled) && onMessage)
        onMessage (before == State::lost ? "Your guitar input is back" : "Audio restarted");
    if (s != State::healthy) bluetoothDismissed = false;
}

void DeviceGuard::adopt()
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr || dev->getActiveInputChannels().isZero()) return;
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    if (s.inputDeviceName.isEmpty()) return;
    wanted = { dm.getCurrentAudioDeviceType(), s.inputDeviceName, s.outputDeviceName, dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate() };
    bluetoothName = looksLikeBluetooth (s.outputDeviceName) || looksLikeScreen (s.outputDeviceName) ? s.outputDeviceName : juce::String();
    bluetoothDismissed = false;
    attempts = looks = 0;
    nextTryAt = nextLookAt = 0;
    lastBeat = beats.load();
    lastMovedAt = clock;
    if (guardMuted) { mute.store (false); guardMuted = false; }
    current = State::healthy;
    watched = nullptr;
}

// The app starts and the input it was set up with is not there (an interface that is not
// plugged in yet): that input is still the one to wait for, not whatever opened in its place.
void DeviceGuard::adoptSaved()
{
    const auto saved = dm.createStateXml();
    if (saved == nullptr) return;
    auto input = saved->getStringAttribute ("audioInputDeviceName");
    auto output = saved->getStringAttribute ("audioOutputDeviceName");
    if (saved->getStringAttribute ("audioDeviceName").isNotEmpty()) input = output = saved->getStringAttribute ("audioDeviceName");
    const auto type = saved->getStringAttribute ("deviceType");
    if (input.isEmpty() || type.isEmpty()) return;
    // it opened as saved, or the same input opened another way (shared mode when exclusive would not start)
    if (wanted.valid() && wanted.input == input) return;
    wanted = { type, input, output, saved->getIntAttribute ("audioDeviceBufferSize", 0), saved->getDoubleAttribute ("audioDeviceRate", 0.0) };
    devlog::note ("The saved input did not open at start: " + type + " / " + input);
    current = State::lost;
}

void DeviceGuard::retry()
{
    devlog::note ("The player asked for another go");
    attempts = looks = 0;
    nextTryAt = nextLookAt = 0;
}

void DeviceGuard::pause (bool p)
{
    paused = p;
    watched = nullptr;
    lastBeat = beats.load();
    lastMovedAt = clock;
    if (p && guardMuted) { mute.store (false); guardMuted = false; } // setup does its own muting
}

juce::StringArray DeviceGuard::allInputs() const
{
    // what the drivers already know: Windows tells them when something is plugged in or pulled out
    juce::StringArray names;
    for (auto* t : dm.getAvailableDeviceTypes())
        for (auto& n : t->getDeviceNames (true))
            names.addIfNotAlreadyThere (n);
    return names;
}

bool DeviceGuard::inputAvailable (const juce::String& type, const juce::String& input, bool rescan) const
{
    auto* t = findType (dm, type);
    if (t == nullptr) return false;
    if (rescan) t->scanForDevices();
    return t->getDeviceNames (true).contains (input);
}

bool DeviceGuard::reopen()
{
    if (! wanted.valid()) return false;
    auto* t = findType (dm, wanted.type);
    if (t == nullptr) return false;
    AudioOpen o { wanted.type, wanted.input, wanted.output, wanted.bufferSize, wanted.sampleRate };
    const auto outputs = t->getDeviceNames (false);
    if (wanted.type == "ASIO") o.output = wanted.input;
    else if (! outputs.contains (wanted.output))
    {
        const int idx = t->getDefaultDeviceIndex (false);
        o.output = juce::isPositiveAndBelow (idx, outputs.size()) ? outputs[idx] : outputs[0];
    }
    auto err = openExactly (dm, o);
    if (err.isNotEmpty() && (o.bufferSize != 0 || o.sampleRate > 0))
    {
        // the exact buffer or rate may be gone (another app holds the device): let the driver choose
        o.bufferSize = 0;
        o.sampleRate = 0;
        err = openExactly (dm, o);
    }
    if (err.isNotEmpty() && wanted.type.startsWith ("Windows Audio (") && findType (dm, "Windows Audio") != nullptr
        && inputAvailable ("Windows Audio", wanted.input, false))
    {
        // The fast mode will not start (another app may hold the card). Shared mode nearly always
        // does: a little more delay beats no sound.
        o.type = "Windows Audio";
        err = openExactly (dm, o);
        if (err.isEmpty() && dm.getCurrentAudioDevice() != nullptr)
        {
            wanted.type = o.type;
            wanted.bufferSize = 0;
            wanted.sampleRate = 0;
            if (onMessage) onMessage ("The fast audio mode would not start, so Riffhouse is on shared mode for now");
        }
    }
    lastBeat = beats.load();
    lastMovedAt = clock;
    watched = nullptr;
    return err.isEmpty() && dm.getCurrentAudioDevice() != nullptr;
}

// One go at getting the sound card back, if it is time for one. Each go waits longer than the
// one before, and after the last the card is left alone.
bool DeviceGuard::tryNow (double now)
{
    if (attempts >= kMaxAttempts || now < nextTryAt) return false;
    // a card that answers slowly gets asked less often still, so the window is never mostly waiting
    const double spent = devlog::busyMs (30000.0);
    nextTryAt = now + std::max (kWaitMs[attempts], 6.0 * spent);
    ++attempts;
    lastTryAt = now;
    if (attempts == kMaxAttempts) devlog::note ("That was the last go by itself. Waiting for a change or for the player.");
    dm.closeAudioDevice();
    return reopen();
}

void DeviceGuard::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Only a note to look on the next tick. Doing the work here would run it once per message,
    // and opening a device sends messages of its own.
    listChanged = true;
}

void DeviceGuard::timerCallback() { tick (nowMs()); }

void DeviceGuard::tick (double now)
{
    clock = now;
    const auto beat = beats.load();
    const bool moving = beat != lastBeat;
    lastBeat = beat;
    if (moving) lastMovedAt = now;
    const bool changed = std::exchange (listChanged, false);
    if (paused) { lastMovedAt = now; return; }

    auto* dev = dm.getCurrentAudioDevice();
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);

    if (! wanted.valid())
    {
        // Nothing has ever been set up. Watch for an input being plugged in.
        setState (dev != nullptr && ! dev->getActiveInputChannels().isZero() ? State::healthy : State::noDevice);
        if (current == State::healthy) { adopt(); return; }
        if (changed)
        {
            const auto inputsNow = allInputs();
            bool fresh = false;
            for (auto& n : inputsNow)
                if (! knownInputs.contains (n)) fresh = true;
            knownInputs = inputsNow;
            if (fresh && onNeedSetup) onNeedSetup();
        }
        return;
    }

    const bool ours = dev != nullptr && s.inputDeviceName == wanted.input && dm.getCurrentAudioDeviceType() == wanted.type;
    if (ours)
    {
        if (moving)
        {
            // only a card that has kept going for a while gets a fresh set of goes: one that starts
            // and stops again is not restarted for ever
            if (now - lastTryAt > 20000.0) { attempts = 0; nextTryAt = 0; }
            looks = 0;
            nextLookAt = 0;
            if (guardMuted) { mute.store (false); guardMuted = false; }
            setState (State::healthy);
            watchDropouts (*dev, now);
            return;
        }
        if (now - lastMovedAt < kStallMs) return;
        // Open but silent: sleep and wake, or a driver that gave up. Start it again, a few times.
        setState (State::stalled);
        tryNow (now);
        return;
    }

    // The input is gone, or the system put a different one in its place. Never play through a
    // microphone the player did not choose.
    if (dev != nullptr)
    {
        devlog::note ("Closing " + s.inputDeviceName + ": it is not the input that was chosen (" + wanted.input + ")");
        dm.closeAudioDevice();
    }
    if (! guardMuted) { mute.store (true); guardMuted = true; }
    setState (State::lost);

    // Is it back? Windows Audio hears about plugging and unplugging by itself, so its list is only
    // read. Other drivers have to be asked, which is done less and less often.
    const bool due = now >= nextLookAt;
    if (! changed && ! due) return;
    bool mustAsk = false;
    if (due)
    {
        ++looks;
        nextLookAt = now + (looks < 15 ? 2000.0 : looks < 40 ? 5000.0 : 15000.0);
        mustAsk = ! wanted.type.startsWith ("Windows Audio") || looks % 8 == 0;
    }
    const double began = nowMs();
    const bool there = inputAvailable (wanted.type, wanted.input, mustAsk);
    const double took = nowMs() - began;
    if (took > 150.0)
    {
        devlog::note ("Looking for " + wanted.input + " took " + juce::String (juce::roundToInt (took)) + " ms");
        nextLookAt = std::max (nextLookAt, now + 25.0 * took);
    }
    if (! there)
    {
        // gone for real: when it comes back it gets a fresh set of goes
        attempts = 0;
        nextTryAt = 0;
        return;
    }
    tryNow (now);
}

// A buffer that crackles on this computer is made bigger, a step at a time, and kept. Setup listens
// to each size for a second or two; some computers only crackle after longer, or only on some
// starts (two USB devices whose clocks drift apart, a busy driver), so the app keeps listening.
void DeviceGuard::watchDropouts (juce::AudioIODevice& dev, double now)
{
    if (&dev != watched)
    {
        watched = &dev;
        watchedSince = now;
        lastDropouts = -1;
        dropTimes.clear();
    }
    const int count = dev.getXRunCount();
    if (count < 0) return; // this driver does not count them
    if (now - watchedSince < kDropSettleMs || lastDropouts < 0 || count < lastDropouts) { lastDropouts = count; return; }
    const int fresh = count - std::exchange (lastDropouts, count);
    for (int i = 0; i < std::min (fresh, (int) kDropsToRaise); ++i) dropTimes.push_back (now);
    while (! dropTimes.empty() && now - dropTimes.front() > kDropWindowMs) dropTimes.erase (dropTimes.begin());
    if (dropTimes.size() < kDropsToRaise) return;
    dropTimes.clear();

    const int size = dev.getCurrentBufferSizeSamples();
    int next = 0;
    for (int s : dev.getAvailableBufferSizes())
        if (s >= size + size / 8) { next = s; break; }
    if (next == 0 || next > kLargestRaise || raises >= kMaxRaises)
    {
        if (raises < 1000) devlog::note ("Dropouts at " + juce::String (size) + " samples, and the buffer will not go higher by itself");
        raises = 1000; // said once
        return;
    }
    devlog::note ("Dropouts at " + juce::String (size) + " samples (" + juce::String (count) + " since it opened): going up to " + juce::String (next));
    auto o = AudioOpen::running (dm);
    if (! o.valid()) return;
    o.bufferSize = next;
    if (openExactly (dm, o).isNotEmpty() || dm.getCurrentAudioDevice() == nullptr)
    {
        o.bufferSize = size; // the bigger size would not open: back to what played
        openExactly (dm, o);
        raises = kMaxRaises;
        return;
    }
    ++raises;
    wanted.bufferSize = dm.getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    lastBeat = beats.load();
    lastMovedAt = clock;
    watched = nullptr;
    if (onBufferRaised) onBufferRaised (wanted.bufferSize);
}

DeviceGuard::Notice DeviceGuard::notice() const
{
    if (paused) return {};
    switch (current)
    {
        case State::lost:
            if (gaveUp()) return { wanted.input + " is there but will not start. Close other apps that use it, then try again.", "Try again", true };
            return { "Your guitar input was unplugged. Plug it back in and Riffhouse carries on by itself.", "Use another input", true };
        case State::stalled:
            if (gaveUp()) return { "The sound card stopped answering.", "Start it again", true };
            return { "Audio stopped. Riffhouse is starting it again.", {}, true };
        case State::noDevice:
            return { "No guitar input yet. Plug in and Riffhouse will find it.", "Run setup", false };
        case State::healthy:
            if (bluetoothName.isNotEmpty() && ! bluetoothDismissed)
                return { looksLikeScreen (bluetoothName)
                             ? "The sound goes to a TV or screen, which holds it back to match its picture: a delay you can feel in every app. Headphones or speakers on the PC play in time."
                             : "Bluetooth speakers and headphones add a delay you can feel, in every app. Wired ones play in time.",
                         "Got it", true };
            return {};
    }
    return {};
}

void DeviceGuard::dismissNotice() { bluetoothDismissed = true; }
} // namespace rh
