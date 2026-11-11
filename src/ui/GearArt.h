// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Pictures of the gear taken by the 3D renderer in tools/gear3d: the box of a pedal, the
// cabinet of an amp, a knob standing on its own shadow. The app lays the moving parts over
// them: pointer lines, lights, lettering under the knobs. A piece that has no picture is
// drawn by hand as before, so new gear works before anybody has rendered it.
namespace rh::ui::art
{
// The camera stands where the player does and looks down at the gear at this angle, so a
// part that stands `h` above a face is seen `h * rise` further up the screen.
constexpr float rise = 0.190809f;   // sin 11 degrees
constexpr float squash = 0.981627f; // cos 11 degrees: a round knob is this much lower than wide

bool has (const juce::String& name);
// The picture at full size (premultiplied ARGB), or a null image. The last few stay in memory.
juce::Image picture (const juce::String& name);
// Draws `name` over `dest` as sharp as the screen allows, from a copy shrunk to fit.
// False when there is no such picture.
bool draw (juce::Graphics&, const juce::String& name, juce::Rectangle<float> dest, float opacity = 1.0f);
// One frame of a strip of pictures laid out in rows (`columns` across), drawn over `dest`.
bool drawFrame (juce::Graphics&, const juce::String& name, int frame, int frames, int columns, juce::Rectangle<float> dest);
// A smaller copy, every pixel the average of the ones it covers.
juce::Image shrunk (const juce::Image&, int width, int height);
// "GREEN DRIVE" becomes "green_drive": how a design's name is spelled in a file name.
juce::String slug (const juce::String&);
} // namespace rh::ui::art
