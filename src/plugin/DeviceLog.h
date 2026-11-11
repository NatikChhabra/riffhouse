// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

// A short diary of what the app did with the sound card: what it opened, how long that took,
// what failed, when the window stopped answering and whether the last run ended in a crash.
// It stays on the player's computer. "Save a report" in Settings puts the last lines into the
// report, which is how a problem on somebody else's computer can be understood at all.
namespace rh::devlog
{
// Where the diary is kept, and the app's version for its first line. Until this is called
// lines are only kept in memory (tests, a plugin inside recording software).
void start (const juce::File& folder, const juce::String& version);
void stop();

// How the run before this one ended, as far as the diary can tell. Known once start() ran.
enum class LastRun { fine, crashed, froze, cutShort };
LastRun lastRun();

void note (const juce::String& line);  // any thread
juce::String tail (int maxLines = 80); // the newest lines, oldest first

// Times a piece of work on the sound card and writes it down. While one is alive the freeze
// watch knows what the window is waiting for.
class Timed
{
public:
    explicit Timed (juce::String what);
    ~Timed();
    void result (const juce::String& r) { outcome = r; }
    double ms() const;

private:
    juce::String what, outcome;
    double began;
};

// How long the sound card kept the window waiting in the last `withinMs`. The connection guard
// uses this to stay out of the way when the card is slow to answer.
double busyMs (double withinMs);
} // namespace rh::devlog
