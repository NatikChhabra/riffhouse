// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// The connection guard, against a pretend sound card. 0.1.1 froze its window on real computers
// because a card that would not start was asked again every two seconds, for ever, on the
// thread that draws. No test machine has a sound card that misbehaves on demand, so this one
// is made up: it can refuse to open, open and stay silent, vanish and come back. What is
// counted is how often the guard touches it.

#include "engine/DeviceNames.h"
#include "plugin/AudioOpen.h"
#include "plugin/AudioProbe.h"
#include "plugin/Reports.h"
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
    {
        // An ASIO driver is first started in a test run outside the app. Here the test run is a
        // shell command that answers the way Riffhouse's own does, or never answers at all.
        std::printf ("test runs of a driver\n");
        auto shell = [] (const juce::String& line) -> juce::StringArray
        {
           #if JUCE_WINDOWS
            return { "cmd", "/c", line };
           #else
            return { "/bin/sh", "-c", line };
           #endif
        };
        auto runProbe = [] (const juce::StringArray& cmd, double timeoutMs)
        {
            AudioProbe probe;
            AudioProbe::Result got;
            bool called = false;
            probe.start (cmd, timeoutMs, [&] (AudioProbe::Result r) { got = r; called = true; });
            const double began = juce::Time::getMillisecondCounterHiRes();
            while (! called && juce::Time::getMillisecondCounterHiRes() - began < 20000.0)
            {
                juce::Thread::sleep (20);
                probe.poll();
            }
            return std::make_pair (called, got);
        };
       #if JUCE_WINDOWS
        const auto hang = shell ("ping -n 30 127.0.0.1 >nul");
        const auto ok = shell ("echo RH-PROBE ok 48000 Hz, 256 samples");
        const auto failed = shell ("echo RH-PROBE failed Device didn't start correctly& exit 2");
        const auto silent = shell ("echo RH-PROBE silent It opened but sent no sound& exit 3");
       #else
        const auto hang = shell ("sleep 30");
        const auto ok = shell ("echo 'RH-PROBE ok 48000 Hz, 256 samples'");
        const auto failed = shell ("echo \"RH-PROBE failed Device didn't start correctly\"; exit 2");
        const auto silent = shell ("echo 'RH-PROBE silent It opened but sent no sound'; exit 3");
       #endif
        const double began = juce::Time::getMillisecondCounterHiRes();
        auto [calledHang, hung] = runProbe (hang, 600.0);
        const double took = juce::Time::getMillisecondCounterHiRes() - began;
        CHECK (calledHang && hung.outcome == AudioProbe::Outcome::timedOut, "a driver that never answers is stopped at the time limit");
        CHECK (took < 5000.0, "and the wait ends at the limit, not when the driver is done (%.0f ms)", took);
        auto [calledOk, played] = runProbe (ok, 10000.0);
        CHECK (calledOk && played.ok() && played.detail == "48000 Hz, 256 samples", "a driver that played is known to work (%s)", played.detail.toRawUTF8());
        auto [calledFail, refused] = runProbe (failed, 10000.0);
        CHECK (calledFail && refused.outcome == AudioProbe::Outcome::failed && refused.detail.contains ("didn't start"),
               "a driver that failed says what it said (%s)", refused.detail.toRawUTF8());
        auto [calledSilent, quiet] = runProbe (silent, 10000.0);
        CHECK (calledSilent && quiet.outcome == AudioProbe::Outcome::silent, "a driver that opened but stayed silent is told apart");
        {
            AudioProbe probe;
            bool called = false;
            probe.start (hang, 60000.0, [&] (AudioProbe::Result) { called = true; });
            CHECK (probe.busy(), "a test run is under way");
            probe.cancel();
            CHECK (! probe.busy() && ! called, "cancelling stops it and says nothing");
        }
        {
            // a program that cannot be started: the answer comes later, and cancel still takes it back
            const juce::StringArray missing { "/no/such/riffhouse-probe-program" };
            auto [calledMissing, none] = runProbe (missing, 10000.0);
            CHECK (calledMissing && ! none.ok(), "a test run that cannot start says so, once");
            AudioProbe probe;
            bool called = false;
            probe.start (missing, 10000.0, [&] (AudioProbe::Result) { called = true; });
            probe.cancel();
            for (int i = 0; i < 20; ++i) { juce::Thread::sleep (10); probe.poll(); }
            CHECK (! called, "and once cancelled it says nothing, even when it could not start");
        }
        CHECK (AudioProbe::isProbeCommandLine ("--probe-audio ASIO \"ASIO4ALL v2\"") && ! AudioProbe::isProbeCommandLine ("--snapshots out"),
               "the app knows when it was started only to test a driver");
        CHECK (AudioProbe::commandFor ("ASIO", "ASIO4ALL v2").joinIntoString ("|").endsWith ("|--probe-audio|ASIO|ASIO4ALL v2"),
               "the test run is this program with the driver's name as one argument");

        // what this computer showed about each driver is remembered between runs
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("riffhouse-drivers-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()) + ".settings");
        {
            juce::PropertiesFile::Options o;
            juce::PropertiesFile props (file, o);
            AudioProbe::useSettings (&props);
            AudioProbe::remember ("ASIO4ALL v2", false);
            CHECK (AudioProbe::knownBad ("ASIO4ALL v2") && ! AudioProbe::knownBad ("Focusrite USB ASIO"), "a driver that failed is left out of automatic setup");
            AudioProbe::remember ("Focusrite USB ASIO", true);
            CHECK (AudioProbe::provedThisRun ("Focusrite USB ASIO") && ! AudioProbe::provedThisRun ("ASIO4ALL v2"), "one that played is not tested again this run");
            AudioProbe::remember ("ASIO4ALL v2", true);
            CHECK (! AudioProbe::knownBad ("ASIO4ALL v2"), "and picking a failed one by hand, when it then works, clears it");
            AudioProbe::remember ("ASIO4ALL v2", false);
            AudioProbe::useSettings (nullptr);
        }
        {
            juce::PropertiesFile::Options o;
            juce::PropertiesFile props (file, o);
            AudioProbe::useSettings (&props);
            CHECK (AudioProbe::knownBad ("ASIO4ALL v2"), "a failure is still known after the app is closed and opened");
            AudioProbe::useSettings (nullptr);
        }
        file.deleteFile();
        AudioProbe::Result r { AudioProbe::Outcome::timedOut, "It did not answer within 12 seconds", 12000.0 };
        CHECK (AudioProbe::describe (r, "ASIO4ALL v2").contains ("did not answer"), "the diary says a driver was stopped");
    }
    {
        std::printf ("reports and slow outputs\n");
        const juce::String raw = "Recordings in C:\\Users\\Meera\\Music\\Riffhouse Recordings\nCrash in /home/meera/riffhouse\n"
                                 "Signed in as Meera Iyer on DESKTOP-MEERA7\nMEERA wrote this";
        const auto clean = reports::scrub (raw, { "Meera", "Meera Iyer", "Iyer", "DESKTOP-MEERA7" });
        CHECK (! clean.containsIgnoreCase ("meera") && ! clean.containsIgnoreCase ("iyer"), "no trace of the name is left: %s", clean.toRawUTF8());
        CHECK (clean.contains ("C:\\Users\\<you>\\Music") && clean.contains ("/home/<you>/riffhouse"), "user folders in paths are replaced");
        CHECK (clean.contains ("Riffhouse Recordings"), "and the rest of the path stays readable");
        const auto devices = reports::scrub (juce::String (juce::CharPointer_UTF8 ("Output: Headphones (Asha's AirPods Pro)\nOutput: Speakers (Ravi\xe2\x80\x99s JBL)\n"
                                                                                       "mail ravi.k@example.com\nIt's the driver's buffer. Riffhouse's setup")), {});
        CHECK (! devices.contains ("Asha") && ! devices.contains ("Ravi") && devices.contains ("<you>'s AirPods Pro"),
               "a name in a device name is taken out: %s", devices.toRawUTF8());
        CHECK (devices.contains ("mail <email>"), "so is an email address");
        CHECK (devices.contains ("It's the driver's buffer. Riffhouse's setup"), "ordinary words before 's stay");
        const auto accents = reports::scrub (juce::String (juce::CharPointer_UTF8 ("Output: Headphones (Zo\xc3\xab's AirPods)\nOutput: Speakers (Jos\xc3\xa9's JBL)\n"
                                                                                     "Output: \xe0\xa4\xb0\xe0\xa4\xbe\xe0\xa4\xae's Buds\nThe 2's buffer")), {});
        CHECK (! accents.contains ("Zo") && ! accents.contains ("Jos") && accents.contains ("(<you>'s JBL)") && ! accents.contains (juce::CharPointer_UTF8 ("\xe0\xa4\xb0")),
               "names with accents or in another script are taken out too: %s", accents.toRawUTF8());
        CHECK (accents.contains ("The 2's buffer"), "a number before 's stays");
        const auto edges = reports::scrub (juce::String (juce::CharPointer_UTF8 ("Headphones (ASHA'S AIRPODS)\nSpeakers (Ravi's-JBL)\n"
                                                                                  "\xe2\x80\x9c" "driver's buffer\nSpeakers\xe2\x80\x94Riffhouse's setup")), {});
        CHECK (! edges.contains ("ASHA") && ! edges.contains ("Ravi"), "capital letters and a dash after 's do not hide a name: %s", edges.toRawUTF8());
        CHECK (edges.contains ("driver's buffer") && edges.contains ("Riffhouse's setup"), "and curly quotes or dashes are not taken for letters");
        CHECK (reports::scrub ("AMP on", { "am" }) == "AMP on", "a two-letter name is not hunted, it would eat ordinary words");
        CHECK (reports::weekOf (juce::Time (2026, 10, 12, 12, 0)) == "2026-W46", "12 November 2026 is in week 46 (%s)", reports::weekOf (juce::Time (2026, 10, 12, 12, 0)).toRawUTF8());
        CHECK (reports::weekOf (juce::Time (2027, 0, 1, 12, 0)) == "2026-W53", "1 January 2027 still belongs to 2026's last week (%s)", reports::weekOf (juce::Time (2027, 0, 1, 12, 0)).toRawUTF8());
        CHECK (reports::weekOf (juce::Time (2026, 0, 1, 12, 0)) == "2026-W01", "1 January 2026, a Thursday, starts week 1");
        {
            // the weekly count: on by default, a random id made once and kept, and nothing else in what is sent
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("riffhouse-hello-test.settings");
            file.deleteFile();
            juce::PropertiesFile::Options o;
            juce::PropertiesFile props (file, o);
            CHECK (reports::helloOn (props), "the weekly count is on for a player who never touched it");
            reports::setHelloOn (props, false);
            CHECK (! reports::helloOn (props), "and off once switched off");
            const auto id = reports::installId (props);
            CHECK (reports::isInstallId (id), "the install id is 32 hex digits: %s", id.toRawUTF8());
            CHECK (reports::installId (props) == id, "and the same the next time");
            juce::PropertiesFile other (file.getSiblingFile ("riffhouse-hello-test2.settings"), o);
            CHECK (reports::installId (other) != id, "another install gets another id");
            props.setValue ("installId", "DESKTOP-MEERA7");
            CHECK (reports::installId (props) != "DESKTOP-MEERA7" && reports::isInstallId (reports::installId (props)), "a kept id that is not random-looking is replaced");
            const auto parsed = juce::JSON::parse (reports::helloJson ("2026-W46", id, "0.1.5"));
            CHECK (parsed["kind"] == "hello" && parsed["week"] == "2026-W46" && parsed["id"] == id && parsed["version"] == "0.1.5", "the hello says the week, id and version");
            CHECK (parsed.getDynamicObject()->getProperties().size() == 5, "and nothing more than those and the app key");
            file.deleteFile();
            file.getSiblingFile ("riffhouse-hello-test2.settings").deleteFile();
        }

        CHECK (DeviceGuard::looksLikeBluetooth ("Speakers (SRS-XP500)"), "a Sony party speaker is known as Bluetooth");
        CHECK (DeviceGuard::looksLikeBluetooth ("Headphones (boAt Rockerz 450 Stereo)"), "and so are boAt headphones");
        CHECK (DeviceGuard::looksLikeScreen ("4 - Mi TV (AMD High Definition Audio Device)"), "a TV over HDMI is known as a screen");
        CHECK (! DeviceGuard::looksLikeBluetooth ("Headphones (4- CX31993+97220 HiFi)") && ! DeviceGuard::looksLikeScreen ("Headphones (4- CX31993+97220 HiFi)"),
               "a wired USB headphone dongle is neither");
        CHECK (! DeviceGuard::looksLikeScreen ("Headphones (Samsung USB C Earphones)") && ! DeviceGuard::looksLikeBluetooth ("Headphones (Samsung USB C Earphones)"),
               "wired Samsung earphones are neither a TV nor Bluetooth");
        CHECK (! DeviceGuard::looksLikeBluetooth ("Speakers (Marshall CODE amp)") && DeviceGuard::looksLikeBluetooth ("Headphones (Marshall Major IV)") && DeviceGuard::looksLikeBluetooth ("Headphones (Marshall Monitor II A.N.C.)"),
               "a Marshall amp over USB is wired, Marshall Major headphones are Bluetooth");
        CHECK (! DeviceGuard::looksLikeBluetooth ("Speakers (USB Audio Stereo)") && DeviceGuard::looksLikeBluetooth ("Headphones (Rockerz 450 Stereo)"),
               "\"Stereo\" marks Bluetooth only when the device is not USB");
        CHECK (DeviceGuard::looksLikeScreen ("Samsung TV (NVIDIA High Definition Audio)") || DeviceGuard::looksLikeScreen ("[TV] Samsung (Intel(R) Display Audio)"),
               "a Samsung TV is still a screen");
        CHECK (delayFeel (12.0) == "instant" && delayFeel (48.0) == "noticeable" && delayFeel (125.0) == "slow", "the delay word is short for slow figures");
        CHECK (! DeviceGuard::looksLikeScreen ("Speakers (USBAudio2.0)") && ! DeviceGuard::looksLikeBluetooth ("Speaker (Realtek(R) Audio)"), "nor are wired speakers");
        CHECK (DeviceGuard::looksLikeVirtual ("Voicemeeter Input (VB-Audio Voicemeeter VAIO)"), "a virtual mixer is not a place to listen");
    }
#undef CHECK
}
} // namespace rh::test
