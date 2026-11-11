// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Updater.h"

namespace rh
{
namespace
{
    const char* const kLatestRelease = "https://api.github.com/repos/NatikChhabra/riffhouse/releases/latest";
    const char* const kAllReleases = "https://api.github.com/repos/NatikChhabra/riffhouse/releases?per_page=20";
    constexpr juce::int64 kMaxInstallerBytes = 400LL * 1024 * 1024;
    constexpr int kFirstCheckMs = 10000;
    constexpr int kRecheckMs = 6 * 60 * 60 * 1000;
    constexpr juce::int64 kMinGapMs = 45LL * 60 * 1000; // launching ten times in a row asks GitHub once

   #if JUCE_WINDOWS
    const char* const kInstallerSuffix = "-windows-setup.exe";
   #else
    const char* const kInstallerSuffix = "";
   #endif
} // namespace

juce::File Updater::updatesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Riffhouse").getChildFile ("updates");
}

Updater::Updater (juce::PropertiesFile& s, juce::String v) : juce::Thread ("Riffhouse update check"), settings (s), current (std::move (v)) {}

Updater::~Updater()
{
    stopTimer();
    cancelPendingUpdate();
    stopThread (4000);
}

bool Updater::automatic() const { return settings.getBoolValue ("autoUpdate", true); }

void Updater::setAutomatic (bool on)
{
    settings.setValue ("autoUpdate", on);
    settings.saveIfNeeded();
    if (on && state() == State::available) checkNow(); // fetch what was only being announced
}

Updater::State Updater::state() const { const juce::ScopedLock sl (lock); return current_state; }
ReleaseInfo Updater::release() const { const juce::ScopedLock sl (lock); return info; }
bool Updater::canInstall() const { const juce::ScopedLock sl (lock); return current_state == State::ready && readyFile.existsAsFile(); }

void Updater::set (State s)
{
    { const juce::ScopedLock sl (lock); current_state = s; }
    triggerAsyncUpdate();
}

juce::String Updater::justUpdatedTo()
{
    const auto last = settings.getValue ("lastRunVersion");
    if (last == current) return {};
    settings.setValue ("lastRunVersion", current);
    settings.saveIfNeeded();
    clearOldDownloads ({});
    return last.isNotEmpty() && compareVersions (current, last) > 0 ? current : juce::String();
}

void Updater::start()
{
    startTimer (kFirstCheckMs);
}

void Updater::timerCallback()
{
    startTimer (kRecheckMs);
    if (isThreadRunning()) return;
    const auto last = settings.getValue ("lastUpdateCheck").getLargeIntValue();
    const auto now = juce::Time::currentTimeMillis();
    if (now - last < kMinGapMs && now >= last && state() != State::idle) return;
    userAsked = false;
    startThread (juce::Thread::Priority::background);
}

void Updater::checkNow()
{
    if (isThreadRunning()) return;
    userAsked = true;
    startThread (juce::Thread::Priority::background);
}

juce::String Updater::fetch (const juce::String& url, int& status)
{
    status = 0;
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (12000)
                             .withNumRedirectsToFollow (5)
                             .withStatusCode (&status)
                             .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: Riffhouse-Updater\r\nX-GitHub-Api-Version: 2022-11-28");
    auto in = juce::URL (url).createInputStream (options);
    if (in == nullptr || status != 200) return {};
    juce::MemoryOutputStream out;
    char buffer[8192];
    while (! in->isExhausted() && ! threadShouldExit() && out.getDataSize() < 2 * 1024 * 1024)
    {
        const int n = in->read (buffer, (int) sizeof (buffer));
        if (n <= 0) break;
        out.write (buffer, (size_t) n);
    }
    return out.toUTF8();
}

bool Updater::download (const ReleaseInfo& r, const juce::File& target)
{
    if (r.size <= 0 || r.size > kMaxInstallerBytes) return false;
    const auto part = target.getSiblingFile (target.getFileName() + ".part");
    part.deleteFile();
    if (! target.getParentDirectory().createDirectory()) return false;
    // the disk must have room for the installer and for what it unpacks
    const auto freeBytes = target.getParentDirectory().getBytesFreeOnVolume();
    if (freeBytes > 0 && freeBytes < r.size * 3) return false;

    int status = 0;
    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (15000)
                             .withNumRedirectsToFollow (8)
                             .withStatusCode (&status)
                             .withExtraHeaders ("Accept: application/octet-stream\r\nUser-Agent: Riffhouse-Updater");
    auto in = juce::URL (r.assetUrl).createInputStream (options);
    if (in == nullptr || status != 200) return false;
    {
        juce::FileOutputStream out (part);
        if (! out.openedOk()) return false;
        juce::HeapBlock<char> buffer (65536);
        juce::int64 total = 0;
        while (! in->isExhausted())
        {
            if (threadShouldExit()) { out.flush(); return false; }
            const int n = in->read (buffer, 65536);
            if (n <= 0) break;
            total += n;
            if (total > r.size || ! out.write (buffer, (size_t) n)) { total = -1; break; }
            downloadProgress = (float) ((double) total / (double) r.size);
        }
        out.flush();
        if (total != r.size) { part.deleteFile(); return false; }
    }
    if (sha256Hex (part) != r.sha256) { part.deleteFile(); return false; } // not the file that was published
    target.deleteFile();
    return part.moveFileTo (target);
}

void Updater::clearOldDownloads (const juce::File& keep)
{
    for (const auto& f : updatesFolder().findChildFiles (juce::File::findFiles, false))
        if (f != keep) f.deleteFile();
}

void Updater::run()
{
    const bool asked = userAsked.load();
    set (State::checking);
    int status = 0;
    // a test build follows the test releases too; a full release only ever follows full releases
    const bool tester = isPreRelease (current);
    const auto json = fetch (tester ? kAllReleases : kLatestRelease, status);
    if (threadShouldExit()) return;
    const auto answer = readCheckAnswer (status, json);
    if (answer == CheckAnswer::unreachable)
    {
        // offline, or GitHub is busy: say so only when the player pressed the button
        set (asked ? State::failed : State::idle);
        return;
    }
    settings.setValue ("lastUpdateCheck", juce::String (juce::Time::currentTimeMillis()));
    if (answer == CheckAnswer::nothingPublished)
    {
        // no full release yet (a test release does not count): this version is the newest there is
        { const juce::ScopedLock sl (lock); info = {}; }
        clearOldDownloads ({});
        set (State::upToDate);
        return;
    }

    const auto latest = tester ? newestRelease (json, kInstallerSuffix) : parseRelease (json, kInstallerSuffix);
    { const juce::ScopedLock sl (lock); info = latest; }
    if (latest.version.isEmpty() || compareVersions (latest.version, current) <= 0)
    {
        clearOldDownloads ({});
        set (State::upToDate);
        return;
    }

   #if JUCE_WINDOWS
    if (automatic() && latest.hasInstaller())
    {
        const auto target = updatesFolder().getChildFile (latest.assetName);
        clearOldDownloads (target);
        bool good = target.existsAsFile() && sha256Hex (target) == latest.sha256; // already fetched on an earlier run
        if (! good)
        {
            downloadProgress = 0.0f;
            set (State::downloading);
            good = download (latest, target);
            if (threadShouldExit()) return;
        }
        if (good)
        {
            { const juce::ScopedLock sl (lock); readyFile = target; }
            set (State::ready);
            return;
        }
    }
   #endif
    set (State::available);
}

bool Updater::launchInstaller (bool relaunch)
{
   #if JUCE_WINDOWS
    juce::File file;
    juce::String expected;
    {
        const juce::ScopedLock sl (lock);
        if (current_state != State::ready) return false;
        file = readyFile;
        expected = info.sha256;
    }
    // check again at the last moment: the file has been sitting on disk since it was downloaded
    if (expected.length() != 64 || sha256Hex (file) != expected)
    {
        file.deleteFile();
        set (State::available);
        return false;
    }
    juce::String args ("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-");
    if (relaunch) args << " /RELAUNCH=1";
    return file.startAsProcess (args);
   #else
    juce::ignoreUnused (relaunch);
    return false;
   #endif
}

juce::String Updater::statusText() const
{
    const juce::ScopedLock sl (lock);
    switch (current_state)
    {
        case State::idle:        return "Version " + current;
        case State::checking:    return "Version " + current + ". Checking for a newer one";
        case State::upToDate:    return "Version " + current + " is the latest";
        case State::downloading: return "Downloading version " + info.version + " (" + juce::String (juce::roundToInt (downloadProgress.load() * 100.0f)) + "%)";
        case State::ready:       return "Version " + info.version + " is ready. It installs when you close Riffhouse";
        case State::available:   return "Version " + info.version + " is out. You have " + current;
        case State::failed:      return "Version " + current + ". Couldn't reach the update server";
    }
    return {};
}
} // namespace rh
