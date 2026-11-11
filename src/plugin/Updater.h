// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/UpdateInfo.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

namespace rh
{
// Keeps the app current without getting in the way.
//  - Checks the project's releases page in the background, a few seconds after launch
//    and every few hours after that. Nothing about the player or their computer is sent.
//  - On Windows a newer installer is downloaded quietly and its SHA-256 is checked against
//    the one GitHub publishes for the file. A file that does not match is deleted, never run.
//  - Nothing is installed while the app is open. The update goes in when the player closes
//    Riffhouse, or when they press "Restart and update". Tones and settings are not touched.
//  - On macOS and Linux it only says a new version is out and links to it.
class Updater : private juce::Thread, private juce::AsyncUpdater, private juce::Timer
{
public:
    enum class State { idle, checking, upToDate, downloading, ready, available, failed };

    Updater (juce::PropertiesFile& settings, juce::String currentVersion);
    ~Updater() override;

    void start();     // begin the quiet schedule
    void checkNow();  // the player asked

    State state() const;
    ReleaseInfo release() const;
    float progress() const noexcept { return downloadProgress.load(); }
    juce::String statusText() const;
    juce::String currentVersion() const { return current; }

    bool automatic() const;
    void setAutomatic (bool);

    bool canInstall() const;                 // a checked installer is waiting
    bool launchInstaller (bool relaunch);    // true once the installer is running: quit now
    juce::String justUpdatedTo();            // non-empty once, on the first launch after an update

    std::function<void()> onChange;          // message thread

    static juce::File updatesFolder();

private:
    void run() override;
    void handleAsyncUpdate() override { if (onChange) onChange(); }
    void timerCallback() override;
    void set (State);
    juce::String fetch (const juce::String& url);
    bool download (const ReleaseInfo&, const juce::File& target);
    void clearOldDownloads (const juce::File& keep);

    juce::PropertiesFile& settings;
    const juce::String current;
    mutable juce::CriticalSection lock;
    State current_state = State::idle;
    ReleaseInfo info;
    juce::File readyFile;
    std::atomic<float> downloadProgress { 0.0f };
    std::atomic<bool> userAsked { false };
};
} // namespace rh
