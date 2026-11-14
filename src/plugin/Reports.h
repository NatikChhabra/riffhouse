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

// Sends straight away on this thread and returns what went wrong, or nothing. For the
// "--report-check" run that proves a release build reaches the relay; not limited per day.
juce::String sendNow (const juce::String& title, const juce::String& body);

// The weekly hello: on unless the player switches it off in Settings. Once a week at most it
// sends the week, a random install id and the app version, and nothing else.
bool helloOn (juce::PropertiesFile&);
void setHelloOn (juce::PropertiesFile&, bool);
void sayHelloIfDue (juce::PropertiesFile&, const juce::String& version);
juce::String weekOf (juce::Time); // "2026-W46"
// Test machines and builds from source never count: only a release copy on a player's PC does.
bool countsAsPlayer (const juce::String& version, bool onTestMachine);
bool onTestMachine();
juce::String installId (juce::PropertiesFile&);  // made once from random numbers, then kept
bool isInstallId (const juce::String&);           // 32 lowercase hex digits
juce::String helloJson (const juce::String& week, const juce::String& id, const juce::String& version);
} // namespace rh::reports
