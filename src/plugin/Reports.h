// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

// Reports that reach the people who fix Riffhouse, only after the player says yes. They go to a
// small relay (a Google Apps Script) that files them in a private repository; the relay holds
// the GitHub token, the app never does. Its address is put in at build time (REPORT_URL), so a
// build without it simply has nothing to send to and keeps "Save a report" as before.
namespace rh::reports
{
// The player's Windows user name, full name and computer name, and the user folder in any
// path, are replaced before a report leaves the computer.
juce::String scrub (const juce::String& text, const juce::StringArray& privateWords);
juce::String scrubForThisComputer (const juce::String& text);

bool canSend();                                  // this build knows where to send
bool allowedToday (juce::PropertiesFile&);       // at most 3 reports a day leave one computer
// Sends on a background thread; `done` is called on the message thread with whether it arrived.
void send (juce::PropertiesFile&, const juce::String& title, const juce::String& body, std::function<void (bool, juce::String)> done);

// The weekly hello, only when the player switched it on: adds one to this week's count. Once a week at most.
bool helloOn (juce::PropertiesFile&);
void setHelloOn (juce::PropertiesFile&, bool);
void sayHelloIfDue (juce::PropertiesFile&);
juce::String weekOf (juce::Time); // "2026-W46"
} // namespace rh::reports
