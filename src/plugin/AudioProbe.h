// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

namespace rh
{
// Some ASIO drivers take many seconds to fail, and a wrapper such as ASIO4ALL can hang outright
// when the sound devices under it are busy. Opened inside Riffhouse, that time is spent with the
// window frozen. So a driver is first started in a second copy of Riffhouse that does nothing
// else ("--probe-audio"): the window keeps answering, and a driver that does not answer in time
// is stopped from outside. Only a driver that played there is opened in the app itself.
class AudioProbe : private juce::Timer
{
public:
    enum class Outcome { ok, failed, silent, timedOut, couldNotStart };
    struct Result
    {
        Outcome outcome = Outcome::failed;
        juce::String detail; // what the driver said, in its own words
        double ms = 0;
        bool ok() const noexcept { return outcome == Outcome::ok; }
    };

    AudioProbe() = default;
    ~AudioProbe() override; // a probe still running is stopped; `done` is not called

    static constexpr double kTimeoutMs = 12000.0;
    // Starts `command` and calls `done` once, on the message thread. Message thread only.
    bool start (const juce::StringArray& command, double timeoutMs, std::function<void (Result)> done);
    bool busy() const noexcept { return child != nullptr || startFailed; }
    void cancel();
    void poll(); // what the timer does: looks at the test run once (tests call it directly)

    // The command that checks one driver with this same program.
    static juce::StringArray commandFor (const juce::String& type, const juce::String& device);

    // The second copy's side: opens the driver, waits for it to play, closes it and prints one
    // line. Returns the program's exit code.
    static int run (const juce::StringArray& args);
    static bool isProbeCommandLine (const juce::String& commandLine) { return commandLine.contains ("--probe-audio"); }

    // What this computer has shown about each driver. A driver that failed or hung is left out
    // of automatic setup from then on; picking it by hand checks it again, and success clears it.
    static void useSettings (juce::PropertiesFile*);
    static bool knownBad (const juce::String& device);
    static bool provedThisRun (const juce::String& device);
    static void remember (const juce::String& device, bool worked);
    static juce::String describe (const Result&, const juce::String& device);

private:
    void timerCallback() override { poll(); }
    std::unique_ptr<juce::ChildProcess> child;
    std::function<void (Result)> onDone;
    double began = 0, timeout = 0;
    bool startFailed = false;
};
} // namespace rh
