// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// The connection guard, against a pretend sound card. 0.1.1 froze its window on real computers
// because a card that would not start was asked again every two seconds, for ever, on the
// thread that draws. No test machine has a sound card that misbehaves on demand, so this one
// is made up: it can refuse to open, open and stay silent, vanish and come back. What is
// counted is how often the guard touches it.

#include "plugin/AudioOpen.h"
#include "plugin/DeviceGuard.h"
#include "plugin/DeviceLog.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <cstdio>
#include <map>

namespace
{
using namespace rh;

const char* const kFast = "Windows Audio (Exclusive Mode)";
const char* const kShared = "Windows Audio";
const char* const kOld = "DirectSound";
const juce::String kGuitar = "Guitar In (Pretend Interface)", kMic = "Laptop Mic", kSpeakers = "Speakers";

struct World
{
    juce::StringArray inputs { kGuitar, kMic }, outputs { kSpeakers };
    std::map<juce::String, bool> refuses; // by driver type: opening fails
    bool silent = false;                  // opens, but no sound ever arrives
    bool playing = false;
    int opens = 0, scans = 0;
    void forget() { opens = scans = 0; }
};

class PretendDevice : public juce::AudioIODevice
{
public:
    PretendDevice (World& w, const juce::String& type, const juce::String& out, const juce::String& in)
        : juce::AudioIODevice (out.isNotEmpty() ? out : in, type), world (w), input (in) {}
    ~PretendDevice() override { close(); }

    juce::StringArray getOutputChannelNames() override { return { "Left", "Right" }; }
    juce::StringArray getInputChannelNames() override { return input.isNotEmpty() ? juce::StringArray { "1", "2" } : juce::StringArray(); }
    juce::Array<double> getAvailableSampleRates() override { return { 44100.0, 48000.0 }; }
    juce::Array<int> getAvailableBufferSizes() override { return { 64, 128, 256, 512 }; }
    int getDefaultBufferSize() override { return 256; }
    juce::String open (const juce::BigInteger& ins, const juce::BigInteger& outs, double rate, int buffer) override
    {
        ++world.opens;
        close();
        if (world.refuses[getTypeName()] || (input.isNotEmpty() && ! world.inputs.contains (input)))
            return error = "The pretend card is busy";
        error = {};
        activeIn = input.isNotEmpty() ? ins : juce::BigInteger();
        activeOut = outs;
        sampleRate = rate > 0 ? rate : 48000.0;
        bufferSize = buffer > 0 ? buffer : 256;
        opened = true;
        return {};
    }
    void close() override { stop(); opened = false; }
    bool isOpen() override { return opened; }
    void start (juce::AudioIODeviceCallback* cb) override
    {
        if (! opened) return;
        callback = cb;
        if (cb != nullptr) cb->audioDeviceAboutToStart (this);
        world.playing = true;
    }
    void stop() override
    {
        if (auto* cb = std::exchange (callback, nullptr)) cb->audioDeviceStopped();
        if (opened) world.playing = false;
    }
    bool isPlaying() override { return callback != nullptr; }
    juce::String getLastError() override { return error; }
    int getCurrentBufferSizeSamples() override { return bufferSize; }
    double getCurrentSampleRate() override { return sampleRate; }
    int getCurrentBitDepth() override { return 24; }
    juce::BigInteger getActiveOutputChannels() const override { return activeOut; }
    juce::BigInteger getActiveInputChannels() const override { return activeIn; }
    int getOutputLatencyInSamples() override { return bufferSize; }
    int getInputLatencyInSamples() override { return bufferSize; }

private:
    World& world;
    juce::String input, error;
    juce::BigInteger activeIn, activeOut;
    juce::AudioIODeviceCallback* callback = nullptr;
    double sampleRate = 48000.0;
    int bufferSize = 256;
    bool opened = false;
};

class PretendType : public juce::AudioIODeviceType
{
public:
    PretendType (World& w, const juce::String& name) : juce::AudioIODeviceType (name), world (w) { scanForDevices(); }
    // Windows Audio is told by Windows what is plugged in; the older kinds only know what they last asked
    bool told() const { return getTypeName().startsWith ("Windows Audio"); }
    void scanForDevices() override { ++world.scans; ins = world.inputs; outs = world.outputs; }
    juce::StringArray getDeviceNames (bool wantInputs) const override { return told() ? (wantInputs ? world.inputs : world.outputs) : (wantInputs ? ins : outs); }
    int getDefaultDeviceIndex (bool) const override { return 0; }
    int getIndexOfDevice (juce::AudioIODevice* d, bool asInput) const override { return d != nullptr ? getDeviceNames (asInput).indexOf (d->getName()) : -1; }
    bool hasSeparateInputsAndOutputs() const override { return true; }
    juce::AudioIODevice* createDevice (const juce::String& out, const juce::String& in) override { return new PretendDevice (world, getTypeName(), out, in); }

private:
    World& world;
    juce::StringArray ins, outs;
};

struct Manager : juce::AudioDeviceManager
{
    explicit Manager (World& w) : world (w) {}
    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& list) override
    {
        for (auto* n : { kFast, kShared, kOld }) list.add (new PretendType (world, n));
    }
    World& world;
};

// A guard with its own clock and its own count of audio blocks.
struct Bench
{
    explicit Bench (const juce::String& type = kFast)
    {
        dm.initialise (2, 2, nullptr, false); // makes the driver types; nothing is chosen yet
        dm.closeAudioDevice();
        if (type.isNotEmpty()) openExactly (dm, { type, kGuitar, kSpeakers, 128, 48000.0 });
        world.forget();
        guard = std::make_unique<DeviceGuard> (dm, beats, mute);
    }
    // `seconds` of the guard looking twice a second, with audio flowing whenever the card plays
    void run (double seconds)
    {
        for (int i = 0; i < (int) (seconds * 2.0); ++i)
        {
            if (world.playing && ! world.silent && dm.getCurrentAudioDevice() != nullptr) ++beats;
            clock += 500.0;
            guard->tick (clock);
        }
    }
    juce::String openInput()
    {
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        return dm.getCurrentAudioDevice() != nullptr ? s.inputDeviceName : juce::String();
    }
    World world;
    Manager dm { world };
    std::atomic<juce::uint32> beats { 0 };
    std::atomic<bool> mute { false };
    double clock = juce::Time::getMillisecondCounterHiRes();
    std::unique_ptr<DeviceGuard> guard;
};

using State = DeviceGuard::State;
} // namespace

namespace rh::test
{
void runDeviceChecks (int& checks, int& failures)
{
#define CHECK(cond, ...)                                                  \
    do {                                                                  \
        ++checks;                                                         \
        if (! (cond)) {                                                   \
            ++failures;                                                   \
            std::printf ("  FAIL %s:%d: ", __FILE__, __LINE__);           \
            std::printf (__VA_ARGS__);                                    \
            std::printf ("\n");                                           \
        }                                                                 \
    } while (0)

    std::printf ("the connection guard\n");
    const juce::ScopedJuceInitialiser_GUI events; // the guard owns a timer; nothing here waits for it

    {
        // all is well: an hour of playing, and the sound card is never touched
        Bench b;
        CHECK (b.openInput() == kGuitar, "the pretend guitar input opens");
        b.run (3600.0);
        CHECK (b.guard->state() == State::healthy, "a card that plays is healthy");
        CHECK (b.world.opens == 0 && b.world.scans == 0, "a healthy card is left alone for an hour (%d opens, %d scans)", b.world.opens, b.world.scans);
        CHECK (! b.mute.load(), "and nothing is muted");
    }
    {
        // a storm of "something changed" messages, which opening a device also sends: no work per message
        Bench b;
        b.run (5.0);
        for (int i = 0; i < 500; ++i) b.guard->somethingChanged();
        b.run (5.0);
        CHECK (b.world.opens == 0, "500 change messages open nothing (%d opens)", b.world.opens);
        CHECK (b.guard->state() == State::healthy, "and the connection stays healthy");
    }
    {
        // the card stops calling back and never plays again, though it opens every time
        Bench b;
        b.run (5.0);
        b.world.silent = true;
        b.run (10.0);
        CHECK (b.guard->state() == State::stalled, "no audio for three seconds is a stall");
        CHECK (b.world.opens >= 1, "which the guard tries to restart");
        b.run (3600.0);
        CHECK (b.guard->gaveUp(), "a card that stays silent is given up on");
        const int after = b.world.opens;
        CHECK (after <= 10, "after at most five goes (%d opens)", after);
        b.run (3600.0);
        CHECK (b.world.opens == after, "and then left alone: no opens in the next hour (%d more)", b.world.opens - after);
        CHECK (b.guard->notice().action.isNotEmpty(), "the player gets a button to start it again");
        // the player presses it, and by now the card works again when it is started afresh
        b.world.silent = false;
        b.world.playing = false;
        b.run (5.0);
        CHECK (b.world.opens == after && b.guard->state() == State::stalled, "until the player says so, nothing is tried");
        b.guard->retry();
        b.run (10.0);
        CHECK (b.guard->state() == State::healthy, "pressed, with the card working again, it is healthy");
        CHECK (b.world.opens > after, "because the guard did try again");
    }
    {
        // a card that starts, plays for a moment and dies, over and over
        Bench b;
        b.run (5.0);
        for (int i = 0; i < 400 && ! b.guard->gaveUp(); ++i)
        {
            b.world.silent = true;
            const int before = b.world.opens;
            for (int t = 0; t < 240 && b.world.opens == before && ! b.guard->gaveUp(); ++t) b.run (0.5); // until it is restarted
            b.world.silent = false;
            b.run (2.0); // plays for two seconds, then dies again
        }
        CHECK (b.guard->gaveUp(), "a card that keeps dying is not restarted for ever");
        CHECK (b.world.opens <= 10, "it gets a handful of goes (%d opens)", b.world.opens);
    }
    {
        // the input is pulled out, and plugged back in two minutes later
        Bench b;
        b.run (5.0);
        b.world.inputs.removeString (kGuitar);
        b.dm.closeAudioDevice(); // what a real card does when its plug is pulled
        b.run (120.0);
        CHECK (b.guard->state() == State::lost, "an input that is gone is lost");
        CHECK (b.mute.load(), "and the rig is muted");
        CHECK (b.world.opens == 0, "nothing is opened while it is gone (%d opens)", b.world.opens);
        CHECK (b.world.scans <= 10, "Windows Audio is told about plugs, so it is hardly asked (%d scans in two minutes)", b.world.scans);
        b.world.inputs.insert (0, kGuitar);
        b.guard->somethingChanged();
        b.run (6.0);
        CHECK (b.openInput() == kGuitar, "plugged back in, it is opened again");
        CHECK (b.guard->state() == State::healthy && ! b.mute.load(), "and plays, unmuted");
        CHECK (b.world.opens <= 2, "with one go (%d opens)", b.world.opens);
    }
    {
        // the same under a driver that is not told about plugs and has to be asked
        Bench b (kOld);
        b.run (5.0);
        b.world.inputs.removeString (kGuitar);
        b.dm.closeAudioDevice();
        b.run (3600.0);
        CHECK (b.guard->state() == State::lost, "gone under the old driver too");
        CHECK (b.world.scans <= 300, "asked less and less often: %d times in an hour", b.world.scans);
        CHECK (b.world.opens == 0, "and nothing opened (%d)", b.world.opens);
        b.world.inputs.insert (0, kGuitar);
        b.run (20.0);
        CHECK (b.guard->state() == State::healthy, "and found again within twenty seconds of coming back");
    }
    {
        // the input is there but the fast mode will not start: shared mode is used instead
        Bench b;
        b.run (5.0);
        b.dm.closeAudioDevice();
        b.world.refuses[kFast] = true;
        b.run (10.0);
        CHECK (b.openInput() == kGuitar && b.dm.getCurrentAudioDeviceType() == kShared, "the fast mode refuses, shared mode takes over (%s)", b.dm.getCurrentAudioDeviceType().toRawUTF8());
        CHECK (b.guard->state() == State::healthy && ! b.mute.load(), "and the guitar is heard");
        const int opens = b.world.opens;
        b.run (3600.0);
        CHECK (b.world.opens == opens, "after which the card is left alone (%d more opens)", b.world.opens - opens);
    }
    {
        // the input is there and nothing at all will open it
        Bench b;
        b.run (5.0);
        b.dm.closeAudioDevice();
        b.world.refuses[kFast] = b.world.refuses[kShared] = true;
        b.run (3600.0);
        CHECK (b.guard->state() == State::lost && b.guard->gaveUp(), "a card that never opens is given up on");
        const int opens = b.world.opens;
        CHECK (opens <= 15, "after five goes (%d opens: each go tries the saved buffer, any buffer, and shared mode)", opens);
        b.run (3600.0);
        CHECK (b.world.opens == opens, "and not asked again by itself (%d more)", b.world.opens - opens);
        CHECK (b.guard->notice().text.contains ("will not start"), "the notice says it will not start");
    }
    {
        // Windows puts another microphone in the guitar's place
        Bench b;
        b.run (5.0);
        b.world.inputs.removeString (kGuitar);
        openExactly (b.dm, { kFast, kMic, kSpeakers, 0, 0.0 }); // stands in for the system doing it
        b.run (3.0);
        CHECK (b.openInput().isEmpty(), "a microphone nobody chose is closed");
        CHECK (b.mute.load() && b.guard->state() == State::lost, "and the rig is muted and waiting for the guitar");
    }
    {
        // the app starts and the saved input is not plugged in: what opened in its place is not adopted
        World w;
        w.inputs = { kMic };
        Manager dm (w);
        juce::XmlElement saved ("DEVICESETUP");
        saved.setAttribute ("deviceType", kFast);
        saved.setAttribute ("audioInputDeviceName", kGuitar);
        saved.setAttribute ("audioOutputDeviceName", kSpeakers);
        dm.initialise (2, 2, &saved, true);
        std::atomic<juce::uint32> beats { 0 };
        std::atomic<bool> mute { false };
        DeviceGuard guard (dm, beats, mute);
        double clock = juce::Time::getMillisecondCounterHiRes();
        auto run = [&] (double seconds)
        {
            for (int i = 0; i < (int) (seconds * 2.0); ++i)
            {
                if (w.playing) ++beats;
                clock += 500.0;
                guard.tick (clock);
            }
        };
        run (3.0);
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        CHECK (dm.getCurrentAudioDevice() == nullptr || s.inputDeviceName != kMic, "the laptop microphone that opened in its place is closed");
        CHECK (guard.state() == State::lost && mute.load(), "the guard waits for the saved input");
        w.inputs.insert (0, kGuitar);
        guard.somethingChanged();
        run (6.0);
        dm.getAudioDeviceSetup (s);
        CHECK (dm.getCurrentAudioDevice() != nullptr && s.inputDeviceName == kGuitar, "and opens it when it is plugged in");
        CHECK (guard.state() == State::healthy && ! mute.load(), "after which the guitar plays");
    }
    {
        // nothing was ever set up: the guard watches and opens nothing by itself
        Bench b ("");
        b.run (600.0);
        CHECK (b.guard->state() == State::noDevice, "with nothing chosen there is no device");
        CHECK (b.world.opens == 0 && b.world.scans == 0, "and nothing is opened or asked (%d opens, %d scans)", b.world.opens, b.world.scans);
    }
    {
        // changing the kind of driver opens the sound card once, and only the one asked for
        World w;
        Manager dm (w);
        dm.initialise (2, 2, nullptr, false);
        dm.closeAudioDevice();
        openExactly (dm, { kShared, kMic, kSpeakers, 0, 0.0 });
        w.forget();
        const auto err = openExactly (dm, { kFast, kGuitar, kSpeakers, 128, 48000.0 });
        juce::AudioDeviceManager::AudioDeviceSetup s;
        dm.getAudioDeviceSetup (s);
        CHECK (err.isEmpty() && dm.getCurrentAudioDeviceType() == kFast && s.inputDeviceName == kGuitar, "another driver type opens what was asked for");
        CHECK (w.opens == 1, "with one open, not the default device first (%d opens)", w.opens);
        CHECK (dm.getCurrentAudioDevice() != nullptr && dm.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == 128, "at the buffer asked for");
        const auto now = AudioOpen::running (dm);
        CHECK (now.valid() && now.type == kFast && now.input == kGuitar && now.bufferSize == 128, "and what runs can be read back");
        w.refuses[kShared] = true;
        const auto bad = openExactly (dm, { kShared, kGuitar, kSpeakers, 0, 0.0 });
        CHECK (bad.isNotEmpty() && dm.getCurrentAudioDevice() == nullptr, "a refusal is reported and leaves nothing half open");
        CHECK (openExactly (dm, now).isEmpty() && AudioOpen::running (dm).input == kGuitar, "and what ran before can be opened again");
    }

    std::printf ("the diary knows how the last run ended\n");
    {
        using devlog::LastRun;
        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("riffhouse-diary-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::fine, "a first run has nothing behind it");
        devlog::stop();
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::fine, "a run that closed by itself is fine");
        devlog::note ("The window stopped answering while: opening the pretend card");
        devlog::start (folder, "test"); // the run above never wrote that it closed
        CHECK (devlog::lastRun() == LastRun::froze, "a run ended while the window did not answer is known as frozen");
        CHECK (devlog::tail (4).contains ("closed while the window was not answering"), "and the diary says so");
        devlog::note ("The window stopped answering while: something slow");
        devlog::note ("The window answers again");
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::cutShort, "a freeze that passed is not blamed for a run that was cut short");
        devlog::note ("Starting is taking more than 15 seconds while: Start: opening the sound card");
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::froze, "a start that never finished counts as frozen");
        folder.getChildFile ("last-crash.txt").replaceWithText ("code c0000005 at 1234");
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::crashed && devlog::tail (4).contains ("c0000005"), "a crash note from the last run is read into the diary");
        CHECK (! folder.getChildFile ("last-crash.txt").existsAsFile(), "and removed, so it is reported once");
        devlog::stop();
        devlog::start (folder, "test");
        CHECK (devlog::lastRun() == LastRun::fine, "and the run after a clean close starts with nothing to report");
        devlog::stop();
        folder.deleteRecursively();
    }
#undef CHECK
}
} // namespace rh::test
