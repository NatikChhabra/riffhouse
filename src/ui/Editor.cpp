// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioPicker.h"
#include "Editor.h"
#include "Motion.h"
#include "../plugin/DeviceLog.h"
#include "../plugin/StandaloneHost.h"
#include "TakesView.h"
#include "SongSheet.h"

namespace rh
{
using namespace ui;

namespace
{
    juce::String formatTime (double seconds)
    {
        const int s = (int) seconds;
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    void drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // the amp-face badge, then the name in its script, like the plate on a combo
        drawBrandLockup (g, r.withSizeKeepingCentre (r.getWidth(), 36.0f));
    }
} // namespace

//==============================================================================
class RecordButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RecordButton (RiffhouseProcessor& p) : proc (p) { setTooltip ("Record (R)"); }
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool rec = proc.recorder().isRecording();
        const bool counting = proc.isCountingIn();
        const bool hover = isMouseOver();
        // pushed to the right, so the little arrow for its options sits right behind the word
        const auto timeFont = font (15.0f, Weight::semibold);
        const juce::String word = counting ? "Ready" : "Rec";
        const float tw = rec ? juce::GlyphArrangement::getStringWidth (timeFont, formatTime (proc.recorder().secondsRecorded())) + 2.0f
                             : capsWidth (word, 12.0f, 0.14f);
        r = r.removeFromRight (std::min (r.getWidth(), 16.0f + 8.0f + tw + 2.0f));
        const auto dot = r.removeFromLeft (16.0f).withSizeKeepingCentre (10.0f, 10.0f);
        if (rec)
        {
            // lit while recording, with a soft glow
            g.setGradientFill (juce::ColourGradient (colours::record.withAlpha (0.5f), dot.getCentreX(), dot.getCentreY(),
                                                     colours::record.withAlpha (0.0f), dot.getCentreX() + 12.0f, dot.getCentreY(), true));
            g.fillEllipse (dot.expanded (12.0f));
            g.setColour (colours::record);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (counting ? colours::record : colours::record.withAlpha (hover ? 1.0f : 0.8f));
            if (counting) g.drawEllipse (dot.reduced (0.75f), 1.5f);
            else g.fillEllipse (dot);
        }
        r.removeFromLeft (8.0f);
        if (rec)
        {
            g.setColour (colours::record);
            g.setFont (timeFont);
            g.drawText (formatTime (proc.recorder().secondsRecorded()), r, juce::Justification::centredLeft);
            return;
        }
        drawCaps (g, word, r, 12.0f, hover || counting ? colours::text : colours::textDim, juce::Justification::centredLeft, 0.14f);
    }

    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    RiffhouseProcessor& proc;
};

//==============================================================================
class PresetBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetBox (RiffhouseProcessor& p) : proc (p) { setTooltip ("Browse all tones"); }
    std::function<void()> onClick;

    void refresh()
    {
        const auto n = proc.currentPresetName();
        const bool m = proc.isModified();
        const int idx = proc.currentPresetIndex();
        if (n != name || m != modified || idx != index)
        {
            name = n;
            modified = m;
            index = idx;
            sub = {};
            // "Inspired by <artist>: <title>" shows the title here and the artist on the line under it
            title = n;
            if (n.startsWith ("Inspired by ") && n.contains (": "))
            {
                title = n.fromFirstOccurrenceOf (": ", false, false);
                sub = n.upToFirstOccurrenceOf (": ", false, false);
            }
            if (idx >= 0)
            {
                const auto& pr = proc.presets().all()[(size_t) idx];
                // under the title: who it is after, with the band when the tone is named after a player
                if (pr.inspiredBy.isNotEmpty())
                    sub = "Inspired by " + pr.inspiredBy + (pr.band.isNotEmpty() && pr.band != pr.inspiredBy ? " (" + pr.band + ")" : juce::String());
                else if (sub.isEmpty())
                    sub = pr.category;
                if (! pr.genres.isEmpty()) sub << ", " << pr.genres.joinIntoString (", ").toLowerCase();
            }
            else if (sub.isEmpty())
                sub = "Not saved yet";
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        // flat: the name of the tone, what it is under it, nothing around it
        const bool hover = isMouseOver();
        auto t = getLocalBounds().toFloat();
        auto top = t.removeFromTop (t.getHeight() * 0.56f);
        const auto nameFont = font (17.0f, Weight::semibold);
        // The name stays centred whatever happens around it: "Edited" hangs off its right
        // end, so touching a knob never makes the title jump.
        const float editedW = capsWidth ("Edited", 10.0f) + 10.0f;
        const float room = top.getWidth() - 2.0f * editedW - 18.0f;
        const float nw = std::min (room, juce::GlyphArrangement::getStringWidth (nameFont, title) + 2.0f);
        auto line = top.withSizeKeepingCentre (nw + 18.0f, top.getHeight());
        g.setFont (nameFont);
        g.setColour (hover ? colours::accentHi : colours::text);
        g.drawText (title, line.removeFromLeft (nw), juce::Justification::bottomLeft, true);
        line.removeFromLeft (4.0f);
        strokeIcon (g, icons::chevronDown(), line.withTrimmedTop (line.getHeight() - 15.0f).withSizeKeepingCentre (11.0f, 11.0f),
                    hover ? colours::text : colours::textFaint, 1.6f);
        if (modified)
            drawCaps (g, "Edited", juce::Rectangle<float> (line.getRight() + 8.0f, top.getY() + 4.0f, editedW, top.getHeight() - 4.0f), 10.0f,
                      colours::lamp, juce::Justification::bottomLeft);
        g.setFont (font (12.0f));
        g.setColour (colours::textFaint);
        g.drawText (sub, t.withTrimmedTop (2.0f), juce::Justification::centredTop, true);
    }

    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    RiffhouseProcessor& proc;
    juce::String name, title, sub;
    bool modified = false;
    int index = -2;
};

//==============================================================================
class Scrim : public juce::Component
{
public:
    std::function<void()> onClick;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};

// Shown while a file is held over the window: says what dropping it will do.
class DropHint : public juce::Component
{
public:
    DropHint() { setInterceptsMouseClicks (false, false); }
    void show (const juce::String& head, const juce::String& line)
    {
        title = head;
        detail = line;
        setVisible (true);
        toFront (false);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0e0d0c).withAlpha (0.82f));
        const auto frame = getLocalBounds().toFloat().reduced (28.0f);
        juce::Path edge;
        edge.addRoundedRectangle (frame, 10.0f);
        juce::Path dashed;
        const float dashes[] = { 10.0f, 8.0f };
        juce::PathStrokeType (2.0f).createDashedStroke (dashed, edge, dashes, 2);
        g.setColour (colours::lamp.withAlpha (0.9f));
        g.fillPath (dashed);
        auto text = getLocalBounds().withSizeKeepingCentre (getWidth() - 160, 110);
        g.setColour (colours::text);
        g.setFont (font (30.0f, Weight::semibold));
        g.drawText (title, text.removeFromTop (48), juce::Justification::centred);
        g.setColour (colours::textDim);
        g.setFont (font (16.0f));
        g.drawFittedText (detail, text.removeFromTop (52), juce::Justification::centredTop, 2);
    }

private:
    juce::String title, detail;
};

class Toast : public juce::Component, private juce::Timer
{
public:
    Toast() { setInterceptsMouseClicks (false, false); }
    void show (const juce::String& t)
    {
        text = t;
        setVisible (true);
        // it comes up from the bar it lies on, stays two and a half seconds (longer for a long
        // line, so it can be read to the end), and fades where it is
        shown.to (1.0f, motion::ui, motion::Curve::out, true);
        motion::run (*this, [this] { return shown.moving(); });
        startTimer (juce::jlimit (2500, 6000, 2500 + 40 * (t.length() - 50)));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        // a strip that lies on the bottom bar's upper edge, the same place every message appears
        const float alpha = shown.get();
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff1d1c1b).withAlpha (alpha));
        g.fillRect (r);
        g.setColour (juce::Colours::white.withAlpha (0.09f * alpha));
        g.fillRect (r.withHeight (1.0f));
        g.setFont (font (14.0f, Weight::medium));
        g.setColour (colours::text.withAlpha (alpha));
        g.drawText (text, r.translated (0.0f, leaving ? 0.0f : 5.0f * (1.0f - alpha)), juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        stopTimer();
        leaving = true;
        shown.to (0.0f, motion::leave, motion::Curve::out, true);
        motion::run (*this, [this] { return shown.moving(); },
                     [this] { if (! shown.moving() && shown.target() <= 0.0f) { setVisible (false); leaving = false; } });
        if (! shown.moving()) { setVisible (false); leaving = false; }
    }
    void visibilityChanged() override { if (! isVisible()) { stopTimer(); shown.snap (0.0f); leaving = false; } }
    juce::String text;
    motion::Glide shown;
    bool leaving = false;
};

// A message about the audio connection that stays until the problem is gone: the input was
// unplugged, audio is restarting, Bluetooth headphones add delay. One optional action.
class NoticeBar : public juce::Component
{
public:
    void set (const StandaloneHost::Notice& n)
    {
        if (n.text == notice.text && n.action == notice.action && n.second == notice.second) return;
        notice = n;
        setVisible (n.text.isNotEmpty());
        repaint();
    }
    float wantedWidth() const
    {
        float w = 52.0f + juce::GlyphArrangement::getStringWidth (font (13.5f, Weight::medium), notice.text);
        if (notice.action.isNotEmpty()) w += 34.0f + juce::GlyphArrangement::getStringWidth (font (13.5f, Weight::semibold), notice.action);
        if (notice.second.isNotEmpty()) w += 22.0f + juce::GlyphArrangement::getStringWidth (font (13.5f, Weight::medium), notice.second);
        return w;
    }
    void paint (juce::Graphics& g) override
    {
        // a strip that lies on the bottom bar's upper edge, with the message in its middle
        const auto band = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff1d1c1b));
        g.fillRect (band);
        g.setColour ((notice.warning ? colours::hot : juce::Colours::white).withAlpha (notice.warning ? 0.5f : 0.09f));
        g.fillRect (band.withHeight (1.0f));
        auto row = band.withSizeKeepingCentre (std::min (band.getWidth() - 32.0f, wantedWidth() - 32.0f), band.getHeight());
        g.setColour (notice.warning ? colours::hot : colours::textDim);
        g.fillEllipse (row.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f));
        row.removeFromLeft (12.0f);
        if (notice.second.isNotEmpty())
        {
            // the quieter choice sits at the far end, after the main one
            g.setFont (font (13.5f, Weight::medium));
            const float sw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), notice.second);
            secondArea = row.removeFromRight (sw + 4.0f);
            g.setColour (overSecond ? colours::text : colours::textDim);
            g.drawText (notice.second, secondArea, juce::Justification::centredRight);
            row.removeFromRight (18.0f);
        }
        else
            secondArea = {};
        if (notice.action.isNotEmpty())
        {
            g.setFont (font (13.5f, Weight::semibold));
            const float aw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), notice.action);
            actionArea = row.removeFromRight (aw + 4.0f);
            g.setColour (over ? colours::text : colours::accent);
            g.drawText (notice.action, actionArea, juce::Justification::centredRight);
            g.fillRect (actionArea.getRight() - aw, actionArea.getCentreY() + 8.5f, aw, 1.0f);
            row.removeFromRight (18.0f);
        }
        else
            actionArea = {};
        g.setColour (colours::text);
        g.setFont (font (13.5f, Weight::medium));
        g.drawText (notice.text, row, juce::Justification::centredLeft);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool o = actionArea.expanded (8.0f).contains (e.position);
        const bool o2 = secondArea.expanded (8.0f).contains (e.position);
        if (o != over || o2 != overSecond)
        {
            over = o;
            overSecond = o2;
            setMouseCursor (o || o2 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override { over = overSecond = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        if (actionArea.expanded (8.0f).contains (e.position) && onAction) onAction();
        else if (secondArea.expanded (8.0f).contains (e.position) && onSecond) onSecond();
    }
    std::function<void()> onAction, onSecond;

private:
    StandaloneHost::Notice notice;
    juce::Rectangle<float> actionArea, secondArea;
    bool overSecond = false;
    bool over = false;
};

//==============================================================================
class RiffhouseEditor::Root : public juce::Component
{
public:
    Root (RiffhouseEditor& e, RiffhouseProcessor& p)
        : editor (e), proc (p), presetBox (p), record (p), chain (p), amp (p),
          space (p, P::space, colours::accent, "Space"), doubler (p, P::doubler, colours::accent, "Doubler"),
          volume (p, P::outLevel, colours::accent, "Volume"),
          browser (p), tuner (p), settings (p), recordPanel (p), metroPanel (p), looperPanel (p), saveDialog (p),
          takeBar (p), takesView (p, takeBar)
    {
        // top bar
        addAndMakeVisible (presetBox);
        presetBox.onClick = [this] { toggleOverlay (browser); };
        for (auto* b : { &prevPreset, &nextPreset })
        {
            b->drawBackground = false;
            addAndMakeVisible (*b);
        }
        prevPreset.setTooltip ("Previous tone (Left arrow)");
        nextPreset.setTooltip ("Next tone (Right arrow)");
        prevPreset.onClick = [this] { stepPreset (-1); };
        nextPreset.onClick = [this] { stepPreset (1); };
        favourite.setTooltip ("Add to favourites");
        favourite.onClick = [this] { proc.toggleFavourite (proc.currentPresetIndex()); refreshTop(); };
        saveButton.setTooltip ("Save this tone (Ctrl+S)");
        saveButton.onClick = [this] { showPopover (saveDialog, saveButton.getBounds(), 360, 206); };
        undoButton.setTooltip ("Undo (Ctrl+Z)");
        undoButton.onClick = [this] { stepHistory (false); };
        redoButton.setTooltip ("Redo (Ctrl+Y)");
        redoButton.onClick = [this] { stepHistory (true); };
        loopButton.setTooltip ("Looper (O records, plays and layers)");
        loopButton.onClick = [this] { showPopover (looperPanel, loopButton.getBounds(), 320, 286); };
        sheetButton.setTooltip ("Chords and tabs: paste a song to play along");
        sheetButton.onClick = [this] { toggleOverlay (sheetView); };
        tunerButton.setTooltip ("Tuner (T)");
        tunerButton.onClick = [this] { toggleOverlay (tuner); };
        metroButton.setTooltip ("Metronome");
        metroButton.onClick = [this] { showPopover (metroPanel, metroButton.getBounds(), 300, 274); };
        settingsButton.setTooltip ("Settings");
        settingsButton.onClick = [this] { toggleOverlay (settings); };
        recordOptions.setTooltip ("Recording options and play-along");
        recordOptions.onClick = [this] { showPopover (recordPanel, recordOptions.getBounds(), 380, 522); };
        record.onClick = [this] { toggleRecording(); };
        for (auto* b : { &favourite, &saveButton, &undoButton, &redoButton, &loopButton, &sheetButton, &tunerButton, &metroButton, &settingsButton, &recordOptions })
        {
            b->drawBackground = false;
            b->iconSize = 18.0f;
            b->activeColour = colours::text;
            addAndMakeVisible (*b);
        }
        recordOptions.iconSize = 13.0f;
        for (auto* b : { &prevPreset, &nextPreset })
        {
            b->iconSize = 16.0f;
            b->iconColour = colours::textFaint;
        }
        addAndMakeVisible (record);

        // main
        addAndMakeVisible (chain);
        chain.onSelect = [this] (int i) { showPage (i); };
        addChildComponent (amp);
        for (int i = 0; i < numModules; ++i)
        {
            if (i == mAmp) { pages.add (nullptr); continue; }
            auto* pg = pages.add (new ModulePage (proc, i));
            addChildComponent (pg);
        }

        // bottom
        for (auto* k : { &space, &doubler, &volume })
        {
            k->setGear (KnobStyle::modern, colours::text, colours::textDim, Face::instrumentSemi, true);
            k->capsLabel = true;
            k->setLabelHeight (16);
            addAndMakeVisible (*k);
        }
        space.setTooltip ("How much room: scales all delay and reverb at once");
        doubler.setTooltip ("Widens the guitar like two takes panned left and right");
        addAndMakeVisible (inMeter);
        addAndMakeVisible (outMeter);
        addAndMakeVisible (levelSpot);
        levelSpot.setTooltip (levelTip());
        levelSpot.onClick = [this]
        {
            proc.setValue (P::autoLevel, proc.value (P::autoLevel) > 0.5f ? 0.0f : 1.0f);
            if (proc.value (P::autoLevel) > 0.5f) proc.levels().rematch();
            levelSpot.setTooltip (levelTip());
            repaint();
        };

        // overlays
        browser.onClose = [this] { closeOverlays(); };
        browser.onLoaded = [this] { refreshTop(); };
        browser.onMessage = [this] (juce::String m) { editor.showToast (m); };
        tuner.onClose = [this] { closeOverlays(); };
        sheetView.onClose = [this] { closeOverlays(); };
        sheetView.onMessage = [this] (juce::String m) { editor.showToast (m); };
        settings.onClose = [this] { closeOverlays(); };
        settings.onRunSetup = [this] { closeOverlays(); editor.showSetupWizard(); };
        settings.onMessage = [this] (const juce::String& m) { editor.showToast (m); };
        settings.onLookChange = [this] (Look l)
        {
            // rebuild after the click has finished, never inside it
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RiffhouseEditor> (&editor), l]
                                             { if (safe != nullptr) safe->applyLook (l, true); });
        };
        saveDialog.onSaved = [this] (juce::String m) { closeOverlays(); refreshTop(); editor.showToast (m); };
        takesView.onClose = [this] { closeOverlays(); };
        takesView.onMessage = [this] (juce::String m) { editor.showToast (m); };
        takesView.onPlay = [this] (const Take& t, bool changeTone)
        {
            takeBar.start (t, changeTone);
            // changing the tone needs the rig in front of you, so the list steps aside
            if (changeTone && takeBar.isChangingTone()) closeOverlays();
        };
        takeBar.onMessage = [this] (juce::String m) { editor.showToast (m); };
        takeBar.onChanged = [this]
        {
            placeOverlays(); // an open page makes room for the strip, or takes it back
            placeBars();
            if (takesView.isVisible()) takesView.refresh();
        };
        recordPanel.onShowTakes = [this] { closePopovers(); toggleOverlay (takesView); };
        metroPanel.onChange = [this] { refreshTop(); };
        scrim.onClick = [this] { closePopovers(); };
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings,
                                    (juce::Component*) &takesView, (juce::Component*) &sheetView, (juce::Component*) &scrim, (juce::Component*) &recordPanel, (juce::Component*) &metroPanel, (juce::Component*) &looperPanel,
                                    (juce::Component*) &saveDialog })
            addChildComponent (c);
        addChildComponent (dropHint);
        addChildComponent (toast);
        addChildComponent (noticeBar);
        addChildComponent (takeBacking);
        addChildComponent (takeBar);
        noticeBar.onAction = [this]
        {
            if (proc.footTeaching() != RiffhouseProcessor::FootTeach::off) { proc.footTeachStop(); noticeTicks = 99; return; }
            if (auto* host = StandaloneHost::get()) host->audioNoticeAction();
        };
        noticeBar.onSecond = [] { if (auto* host = StandaloneHost::get()) host->audioNoticeSecond(); };
        // a report sent from the notice strip is the same one Settings saves
        if (auto* host = StandaloneHost::get())
            host->makeReport = [this] { return settings.reportText(); };
        // the foot controller changed a tone, taught itself a job, or pressed something that lives in this window
        proc.onFootMessage = [this] (const juce::String& m)
        {
            refreshTop();
            if (footStrip) { footStrip = false; noticeBar.set ({}); } // teaching is over: its strip makes room for the result
            editor.showToast (m);
        };
        proc.onFootJob = [this] (FootJob job)
        {
            if (job == FootJob::tuner) { toggleOverlay (tuner); return true; }
            if (job == FootJob::record) { toggleRecording(); return true; }
            return false;
        };

        showPage ((int) proc.uiState.getProperty ("page", (int) mAmp));
        refreshTop();
        motion::attach (*this);
    }

    ~Root() override
    {
        motion::detach (*this);
        proc.onFootMessage = nullptr; // the rig lives on after its window closes
        proc.onFootJob = nullptr;
        proc.footTeachStop();
        if (auto* host = StandaloneHost::get()) host->makeReport = nullptr;
    }

    // -------------------------------------------------------------------------
    void showPage (int i)
    {
        i = juce::jlimit (0, numModules - 1, i);
        // the page that is leaving dissolves over the one that is already there
        if (i != shownPage && shownPage >= 0) motion::ghost (*this, amp.getBounds(), motion::leave, topPage());
        shownPage = i;
        chain.setSelected (i);
        amp.setVisible (i == mAmp);
        for (int k = 0; k < pages.size(); ++k)
            if (pages[k] != nullptr) pages[k]->setVisible (k == i);
        proc.uiState.setProperty ("page", i, nullptr);
    }

    // the page furthest to the front, so a leaving picture lies on the pages and under everything else
    juce::Component* topPage()
    {
        juce::Component* top = &amp;
        for (auto* pg : pages)
            if (pg != nullptr && getIndexOfChildComponent (pg) > getIndexOfChildComponent (top)) top = pg;
        return top;
    }

    // A whole new tone: the rig that was there dissolves into the one that is.
    void toneWillChange()
    {
        if (openOverlay == nullptr) motion::ghost (*this, chain.getBounds().getUnion (amp.getBounds()), motion::leave, topPage());
    }

    void stepPreset (int d)
    {
        const int n = (int) proc.presets().all().size();
        if (n == 0) return;
        const int cur = proc.currentPresetIndex();
        toneWillChange();
        proc.loadPreset (cur < 0 ? 0 : (cur + d + n) % n);
        refreshTop();
        editor.showToast (proc.currentPresetName());
    }

    // What the strip on the bottom bar says while a foot controller is being taught.
    StandaloneHost::Notice footNotice() const
    {
        const auto teach = proc.footTeaching();
        if (teach == RiffhouseProcessor::FootTeach::touch)
            return { "Foot control: turn the knob or flip the switch you want under your foot.", "Cancel", false };
        if (teach == RiffhouseProcessor::FootTeach::move)
            return { juce::String (footTargetName (proc.footControl().learningFor())) + ": now move the pedal or press the switch on your controller.", "Cancel", false };
        return {};
    }

    void toggleRecording()
    {
        closePopovers();
        if (proc.recorder().isRecording() || proc.isCountingIn())
        {
            const auto take = proc.recorder().lastTake();
            proc.stopRecording();
            if (take.existsAsFile()) editor.showToast ("Saved. Press L to listen to your takes.");
            if (takesView.isVisible()) takesView.refresh();
        }
        else
        {
            // a take that is only being listened to would drown the guitar you are about to record
            if (takeBar.isActive() && ! takeBar.isChangingTone()) takeBar.stop();
            const bool countIn = proc.uiState.getProperty ("countIn", true);
            if (! proc.startRecording (countIn))
                editor.showToast ("Couldn't start recording. Check the recordings folder in the options.");
        }
    }

    juce::Component* openOverlay = nullptr;

    void toggleOverlay (juce::Component& c)
    {
        const bool wasOpen = openOverlay == &c;
        closeOverlays();
        if (wasOpen) return;
        openOverlay = &c;
        c.setVisible (true);
        c.toFront (true);
        proc.tunerOpen.store (&c == &tuner);
        tunerButton.setToggleState (&c == &tuner, juce::dontSendNotification);
        settingsButton.setToggleState (&c == &settings, juce::dontSendNotification);
        sheetButton.setToggleState (&c == &sheetView, juce::dontSendNotification);
        placeBars();
        takeBar.toFront (false);
        toast.toFront (false);
    }

    // The pages that open over the rig. A playing take's strip sits above the bottom bar, and an
    // open page ends above the strip, so the strip never covers a button.
    void placeOverlays()
    {
        const int bottom = takeBar.isActive() ? bottomY() - 48 : getHeight();
        const auto overlay = juce::Rectangle<int> (0, topH + 1, getWidth(), bottom - topH - 1);
        for (auto* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings, (juce::Component*) &takesView,
                         (juce::Component*) &sheetView })
            c->setBounds (overlay);
    }

    // The strip for a playing take and the audio notice share one place above the bottom bar.
    void placeBars()
    {
        const bool pageOpen = browser.isVisible() || tuner.isVisible() || settings.isVisible() || takesView.isVisible() || sheetView.isVisible();
        takeBacking.setVisible (takeBar.isActive() && pageOpen);
        if (takeBacking.isVisible())
        {
            takeBacking.setBounds (0, bottomY() - 48, getWidth(), getHeight() - (bottomY() - 48));
            takeBacking.toFront (false);
        }
        if (takeBar.isActive())
        {
            const int w = juce::jmin (getWidth() - 48, 900);
            takeBar.setBounds ((getWidth() - w) / 2, bottomY() - 40, w, 32);
            takeBar.toFront (false);
            noticeBar.setVisible (false);
        }
    }

    void powerOn()
    {
        const double ampAt = chain.powerOn (motion::now() + 90.0);
        if (shownPage == mAmp) amp.powerOn (ampAt);
    }

    void closeOverlays()
    {
        closePopovers();
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings, (juce::Component*) &takesView,
                                    (juce::Component*) &sheetView })
            c->setVisible (false);
        openOverlay = nullptr;
        proc.tunerOpen.store (false);
        tunerButton.setToggleState (false, juce::dontSendNotification);
        settingsButton.setToggleState (false, juce::dontSendNotification);
        sheetButton.setToggleState (false, juce::dontSendNotification);
        takeBacking.setVisible (false);
    }

    void showPopover (Popover& p, juce::Rectangle<int> anchor, int w, int h)
    {
        const bool wasOpen = p.isVisible();
        closePopovers();
        if (wasOpen) return;
        scrim.setBounds (getLocalBounds());
        scrim.setVisible (true);
        scrim.toFront (false);
        const int x = juce::jlimit (12, getWidth() - w - 12, anchor.getCentreX() - w / 2);
        p.setBounds (x, anchor.getBottom() + 8, w, h);
        p.setVisible (true);
        p.toFront (true);
    }

    void closePopovers()
    {
        scrim.setVisible (false);
        for (Popover* p : { (Popover*) &recordPanel, (Popover*) &metroPanel, (Popover*) &looperPanel, (Popover*) &saveDialog })
            p->setVisible (false);
    }

    bool anyPopoverOpen() const { return scrim.isVisible(); }

    // Undo or redo one step of the tone, and say what came back.
    void stepHistory (bool forward)
    {
        motion::announceJump(); // the knob that comes back travels to its place
        const auto what = forward ? proc.redo() : proc.undo();
        if (what.isEmpty()) return;
        refreshTop();
        editor.showToast ((forward ? "Redone: " : "Undone: ") + what);
    }

    void refreshTop()
    {
        presetBox.refresh();
        const int idx = proc.currentPresetIndex();
        const bool fav = idx >= 0 && proc.presets().all()[(size_t) idx].favourite;
        favourite.setEnabled (idx >= 0);
        favourite.setIcon (icons::star (fav), fav);
        favourite.setToggleState (fav, juce::dontSendNotification);
        metroButton.setText (juce::String (juce::roundToInt (proc.metronomeBpm.load())));
        metroButton.setToggleState (proc.metronomeOn.load(), juce::dontSendNotification);
    }

    // -------------------------------------------------------------------------
    void tick()
    {
        // for the pictures in the docs the meters show a guitar being played
        inMeter.setLevel (demoLevels ? 0.2f : proc.meters().gateInPeak.load());
        outMeter.setLevel (demoLevels ? 0.42f : proc.meters().outPeak.load());
        chain.refresh();
        if (++historyTicks >= 8) // about four times a second
        {
            historyTicks = 0;
            proc.historyTick (juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown());
            const auto loopState = proc.looper().state();
            const auto loopColour = loopState == Looper::State::recording ? colours::record
                                  : loopState == Looper::State::overdubbing ? colours::hot
                                  : loopState == Looper::State::playing ? colours::accent : colours::textDim;
            if (loopButton.iconColour != loopColour) { loopButton.iconColour = loopColour; loopButton.repaint(); }
            undoButton.setEnabled (proc.canUndo());
            redoButton.setEnabled (proc.canRedo());
        }
        if (! quietNotices && ++noticeTicks >= 10)
        {
            noticeTicks = 0;
            proc.footTick();
            auto n = footNotice();
            footStrip = n.text.isNotEmpty();
            if (! footStrip)
                if (auto* host = StandaloneHost::get(); host != nullptr && wizard == nullptr && ! takeBar.isActive())
                    n = host->audioNotice();
            noticeBar.set (n);
            if (noticeBar.isVisible()) noticeBar.setBounds (0, bottomY() - 34, getWidth(), 34);
        }
        amp.refresh();
        for (auto* pg : pages) if (pg != nullptr && pg->isVisible()) pg->refresh();
        if (tuner.isVisible()) tuner.refresh();
        presetBox.refresh();
        if (proc.recorder().isRecording() || proc.isCountingIn() || wasRecording)
        {
            record.repaint();
            wasRecording = proc.recorder().isRecording() || proc.isCountingIn();
        }
        const auto status = bottomStatus();
        if (status != lastStatus) { lastStatus = status; repaint (0, bottomY(), getWidth(), getHeight() - bottomY()); }
        if (++frame % 15 == 0) refreshTop();
    }

    // Auto level in the bottom bar: a lamp, what it is doing, and a click switches it.
    struct HotSpot : public juce::Component, public juce::SettableTooltipClient
    {
        HotSpot() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
        std::function<void()> onClick;
    };
    juce::String levelText() const
    {
        if (proc.value (P::autoLevel) < 0.5f) return "Auto level off";
        const float t = proc.levels().trimDb();
        const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
        const auto db = (std::abs (t) < 0.05f ? juce::String() : (t > 0.0f ? juce::String ("+") : minus)) + juce::String (std::abs (t), 1) + " dB";
        return "Auto level " + db;
    }
    juce::String levelTip() const
    {
        if (proc.value (P::autoLevel) < 0.5f)
            return "Auto level is off: every tone is as loud as it was made. Click to have Riffhouse keep them level.";
        return juce::String ("Auto level keeps every tone equally loud, so nothing jumps out when you switch. ")
             + (proc.levels().hasPlayersPhrase() ? "It measures them with your own playing, which it has heard. "
                                                 : "It will measure them with your own playing once it has heard a few seconds of it. ")
             + "Click to switch it off.";
    }

    struct StatusItem { juce::String text; bool lit; };
    std::vector<StatusItem> statusItems() const
    {
        // only what is doing something: a quiet bar says nothing needs a look
        std::vector<StatusItem> s;
        const int mains = proc.meters().mains.load();
        const int humMode = juce::roundToInt (proc.value (P::humFilter));
        if (humMode != 0 && mains > 0) s.push_back ({ juce::String (mains) + " Hz hum removed", true });
        if (proc.value (P::inputFix) > 0.5f) s.push_back ({ "Input fix on", true });
        if (proc.value (P::gateOn) > 0.5f)
        {
            const bool open = proc.meters().gateOpen.load();
            s.push_back ({ juce::String ("Gate ") + (open ? "open" : "closed"), open });
        }
        return s;
    }

    juce::String bottomStatus() const
    {
        juce::String all;
        for (const auto& item : statusItems()) all << item.text << (item.lit ? "*" : "-");
        all << inMeter.peakText() << outMeter.peakText() << levelText();
        return all;
    }

    juce::String latencyText() const
    {
        if (demoLevels || noFigures) return {}; // pictures of the app show no figure that belongs to one computer
        if (auto* host = StandaloneHost::get())
            if (auto* dev = host->deviceManager().getCurrentAudioDevice())
            {
                int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
                if (lat <= 0) lat = 2 * dev->getCurrentBufferSizeSamples();
                lat += proc.getLatencySamples();
                return juce::String (1000.0 * lat / std::max (1.0, dev->getCurrentSampleRate()), 1) + " ms round trip";
            }
        if (proc.getLatencySamples() <= 0) return {}; // inside another app with nothing added: nothing to say
        return "The rig adds " + juce::String (1000.0 * proc.getLatencySamples() / std::max (1.0, proc.currentSampleRate()), 2) + " ms";
    }

    // -------------------------------------------------------------------------
    static constexpr int topH = 56, chainY = 66, chainH = 98, pageY = 176, bottomH = 68;
    // the bottom bar sits on the window's floor, however tall the window is
    int bottomY() const { return getHeight() - bottomH; }

    void resized() override
    {
        // top bar: name on the left, the tone in the middle, tools on the right
        auto top = juce::Rectangle<int> (0, 0, getWidth(), topH).reduced (24, 10);
        logoArea = top.removeFromLeft (180);
        settingsButton.setBounds (top.removeFromRight (36));
        top.removeFromRight (6);
        recordOptions.setBounds (top.removeFromRight (18));
        record.setBounds (top.removeFromRight (80));
        top.removeFromRight (10);
        metroButton.setBounds (top.removeFromRight (66));
        tunerButton.setBounds (top.removeFromRight (34));
        loopButton.setBounds (top.removeFromRight (34));
        sheetButton.setBounds (top.removeFromRight (34));

        // the tone's name sits dead centre between its two arrows; star and save follow on the right
        auto centre = juce::Rectangle<int> (0, 0, 380, topH).withCentre ({ getWidth() / 2, topH / 2 }).reduced (0, 8);
        toneWell = centre;
        prevPreset.setBounds (centre.removeFromLeft (28));
        nextPreset.setBounds (centre.removeFromRight (28));
        auto right = juce::Rectangle<int> (centre.getRight() + 28, centre.getY(), 6 + 30 + 2 + 30, centre.getHeight()).translated (-6, 0);
        right.removeFromLeft (6);
        favourite.setBounds (right.removeFromLeft (30));
        right.removeFromLeft (2);
        saveButton.setBounds (right.removeFromLeft (30));
        presetBox.setBounds (centre.reduced (4, 0));
        // undo and redo stand on the other side of the tone, as far from it as star and save
        auto left = juce::Rectangle<int> (toneWell.getX() - 20 - 62, toneWell.getY(), 62, toneWell.getHeight());
        undoButton.setBounds (left.removeFromLeft (30));
        left.removeFromLeft (2);
        redoButton.setBounds (left.removeFromLeft (30));

        chain.setBounds (0, chainY, getWidth(), chainH);
        const auto page = juce::Rectangle<int> (24, pageY, getWidth() - 48, bottomY() - pageY - 6);
        amp.setBounds (page);
        for (auto* pg : pages) if (pg != nullptr) pg->setBounds (page);

        // bottom bar: input on the left, the three big-picture knobs in the middle, output on the right
        auto bottom = juce::Rectangle<int> (24, bottomY(), getWidth() - 48, getHeight() - bottomY());
        inMeterArea = bottom.removeFromLeft (380);
        inMeter.setBounds (inMeterArea.getX() + 62, inMeterArea.getY() + 17, 226, 13);
        outMeterArea = bottom.removeFromRight (380);
        outMeter.setBounds (outMeterArea.getRight() - 226 - 54, outMeterArea.getY() + 17, 226, 13);
        levelSpot.setBounds (outMeter.getX() - 70, outMeterArea.getY() + 36, 170, 20);
        auto macros = bottom.withSizeKeepingCentre (3 * 92, getHeight() - bottomY());
        for (auto* k : { &space, &doubler, &volume })
            k->setBounds (macros.removeFromLeft (92).withSizeKeepingCentre (64, 58));

        placeOverlays();
        placeBars();
        toast.setBounds (0, bottomY() - 34, getWidth(), 34);
        dropHint.setBounds (getLocalBounds());
        stage = {};
    }

    void paint (juce::Graphics& g) override
    {
        if (! stage.isValid() || stage.getWidth() != getWidth())
        {
            stage = renderStage (getWidth(), getHeight());
            juce::Graphics sg (stage);
            // the bars at top and bottom are the edges of the room, a step darker
            sg.setColour (juce::Colour (0xff0b0b0b));
            sg.fillRect (0, 0, getWidth(), topH);
            sg.fillRect (0, bottomY(), getWidth(), getHeight() - bottomY());
            sg.setColour (juce::Colour (0xff222120));
            sg.fillRect (0, topH, getWidth(), 1);
            sg.fillRect (0, bottomY(), getWidth(), 1);
        }
        g.drawImageAt (stage, 0, 0);
        drawLogo (g, logoArea.toFloat());
        {
            // the tone's name sits in a shallow window, like the display on a rack unit,
            // with its two step buttons at the ends
            const auto well = toneWell.toFloat();
            g.setColour (juce::Colour (0xff151413));
            g.fillRoundedRectangle (well, 3.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, well.getY(),
                                                     juce::Colours::transparentBlack, 0.0f, well.getY() + 7.0f, false));
            g.fillRoundedRectangle (well, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (well.getX() + 3.0f, well.getBottom() - 1.0f, well.getWidth() - 6.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (well.getX() + 28.0f, well.getY() + 6.0f, 1.0f, well.getHeight() - 12.0f);
            g.fillRect (well.getRight() - 29.0f, well.getY() + 6.0f, 1.0f, well.getHeight() - 12.0f);
        }

        // bottom bar: each meter with its name in front and its peak in numbers behind
        const float my = (float) inMeterArea.getY() + 14.0f;
        drawCaps (g, "Input", { (float) inMeterArea.getX(), my, 56.0f, 18.0f }, 11.5f, colours::textDim, juce::Justification::centredLeft, 0.14f);
        drawCaps (g, "Output", { (float) outMeter.getX() - 70.0f, my, 62.0f, 18.0f }, 11.5f, colours::textDim, juce::Justification::centredLeft, 0.14f);
        g.setFont (font (12.0f, Weight::numeric));
        g.setColour (colours::textDim);
        g.drawText (inMeter.peakText(), juce::Rectangle<float> ((float) inMeter.getRight() + 8.0f, my, 50.0f, 18.0f), juce::Justification::centredLeft);
        g.drawText (outMeter.peakText(), juce::Rectangle<float> ((float) outMeter.getRight() + 8.0f, my, 50.0f, 18.0f), juce::Justification::centredLeft);

        // what the input stage is doing, one small lamp each
        float x = (float) inMeterArea.getX();
        const float sy = (float) inMeterArea.getY() + 38.0f;
        for (const auto& item : statusItems())
        {
            const auto lampR = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x + 3.0f, sy + 8.0f });
            if (item.lit)
            {
                g.setGradientFill (juce::ColourGradient (colours::lamp.withAlpha (0.45f), lampR.getCentreX(), lampR.getCentreY(),
                                                         colours::lamp.withAlpha (0.0f), lampR.getCentreX() + 8.0f, lampR.getCentreY(), true));
                g.fillEllipse (lampR.expanded (6.0f));
            }
            g.setColour (item.lit ? colours::lamp : juce::Colour (0xff3b3936));
            g.fillEllipse (lampR);
            const float w = capsWidth (item.text, 11.0f, 0.1f);
            drawCaps (g, item.text, { x + 13.0f, sy, w + 4.0f, 16.0f }, 11.0f, colours::textDim, juce::Justification::centredLeft, 0.1f);
            x += 13.0f + w + 20.0f;
        }
        {
            // auto level, under the meter it works on
            const bool on = proc.value (P::autoLevel) > 0.5f;
            const float lx = (float) levelSpot.getX();
            const auto lampR = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ lx + 3.0f, sy + 8.0f });
            if (on)
            {
                g.setGradientFill (juce::ColourGradient (colours::lamp.withAlpha (0.45f), lampR.getCentreX(), lampR.getCentreY(),
                                                         colours::lamp.withAlpha (0.0f), lampR.getCentreX() + 8.0f, lampR.getCentreY(), true));
                g.fillEllipse (lampR.expanded (6.0f));
            }
            g.setColour (on ? colours::lamp : juce::Colour (0xff3b3936));
            g.fillEllipse (lampR);
            drawCaps (g, levelText(), { lx + 13.0f, sy, 170.0f, 16.0f }, 11.0f, colours::textDim, juce::Justification::centredLeft, 0.1f);
        }
        const int clips = proc.meters().clipEvents.load();
        const auto rightLine = juce::Rectangle<float> ((float) outMeterArea.getX(), sy, 380.0f, 16.0f);
        if (clips > 0)
            drawCaps (g, "Hitting the limiter: lower volume a little", rightLine, 11.0f, colours::warn, juce::Justification::centredRight, 0.1f);
        else
            drawCaps (g, latencyText(), rightLine, 11.0f, colours::textDim, juce::Justification::centredRight, 0.1f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && inMeterArea.contains (e.getPosition()))
            toggleOverlay (settings);
    }

    RiffhouseEditor& editor;
    RiffhouseProcessor& proc;
    juce::Rectangle<int> logoArea, inMeterArea, outMeterArea, toneWell;
    juce::Image stage;

    PresetBox presetBox;
    IconButton prevPreset { icons::chevronLeft() }, nextPreset { icons::chevronRight() };
    IconButton favourite { icons::star (false) }, saveButton { icons::save() };
    IconButton undoButton { icons::undo() }, redoButton { icons::redo() }, loopButton { icons::loop() };
    int historyTicks = 0;
    IconButton sheetButton { icons::chordBox() };
    IconButton tunerButton { icons::tuningFork() }, metroButton { icons::metronome(), "100" }, settingsButton { icons::gear() };
    RecordButton record;
    IconButton recordOptions { icons::chevronDown() };

    ChainStrip chain;
    AmpPage amp;
    juce::OwnedArray<ModulePage> pages;

    Knob space, doubler, volume;
    Meter inMeter, outMeter;
    HotSpot levelSpot;

    PresetBrowser browser;
    TunerView tuner;
    SettingsView settings;
    SongSheetView sheetView;
    Scrim scrim;
    RecordPanel recordPanel;
    MetronomePanel metroPanel;
    LooperPanel looperPanel;
    SaveDialog saveDialog;
    Toast toast;
    DropHint dropHint;
    NoticeBar noticeBar;
    bool footStrip = false; // the strip is showing how to teach a foot controller
    TakeBar takeBar;
    // under the strip while a page is open, so the rig does not show through between them
    struct Backing : juce::Component { void paint (juce::Graphics& g) override { g.fillAll (colours::bg); } } takeBacking;
    TakesView takesView;
    int noticeTicks = 0;
    bool quietNotices = false; // pictures of the app are taken without a message about the missing guitar
    std::unique_ptr<SetupWizard> wizard;
    int shownPage = -1;
    // how each panel arrives and leaves
    motion::Presence inBrowser { browser, motion::Enter::rise }, inTuner { tuner, motion::Enter::rise },
                     inSettings { settings, motion::Enter::rise }, inTakes { takesView, motion::Enter::rise },
                     inScrim { scrim, motion::Enter::fade },
                     inRecord { recordPanel, motion::Enter::drop }, inMetro { metroPanel, motion::Enter::drop },
                     inLooper { looperPanel, motion::Enter::drop }, inSave { saveDialog, motion::Enter::drop };

    juce::String lastStatus;
    bool demoLevels = false;
    bool noFigures = false; // films of the app: live meters, but no delay figure in the bottom bar
    int frame = 0;
    bool wasRecording = false;
};

//==============================================================================
RiffhouseEditor::RiffhouseEditor (RiffhouseProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setLookAndFeel (&lnf);
    ui::setLook (ui::Look::studio); // one look: the gear brings the colour
    lnf.applyColours();
    buildRoot();
    tooltips.setLookAndFeel (&lnf);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, maxWidth * 2, maxHeight * 2);
    const double scale = (double) proc.uiState.getProperty ("scale", 1.0);
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
    setWantsKeyboardFocus (proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    if (auto* host = StandaloneHost::get())
    {
        // Setup opens by itself once, on the first start. After that it is a button in Settings,
        // and a missing input is a line at the bottom of the window, never a setup that comes back.
        const bool done = host->settings().getBoolValue ("setupDone", false);
        // a setup made by an older version is offered a new run, once
        const bool old = done && host->settings().getIntValue ("setupVersion", 1) < AutoSetup::kSetupVersion;
        if (! done || old)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RiffhouseEditor> (this), again = done]
                                             { if (safe != nullptr) safe->showSetupWizard (again); });
    }
    startTimerHz (30);
}

RiffhouseEditor::~RiffhouseEditor()
{
    stopTimer();
    proc.tunerOpen.store (false);
    root.reset();
    setLookAndFeel (nullptr);
    tooltips.setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void RiffhouseEditor::paint (juce::Graphics& g)
{
    if (! firstPaintDone && firstPaintBegan == 0.0) firstPaintBegan = juce::Time::getMillisecondCounterHiRes();
    g.fillAll (colours::bg);
}

void RiffhouseEditor::paintOverChildren (juce::Graphics&)
{
    // The first picture is the slowest one: every piece of gear is drawn for the first time.
    // How long it took goes into the diary, so a slow start can be told from a slow sound card.
    if (firstPaintDone) return;
    firstPaintDone = true;
    devlog::note ("The window's first picture took " + juce::String (juce::roundToInt (juce::Time::getMillisecondCounterHiRes() - firstPaintBegan)) + " ms to draw");
}

void RiffhouseEditor::buildRoot()
{
    if (root != nullptr) removeChildComponent (root.get());
    root.reset();
    root = std::make_unique<Root> (*this, proc);
    root->setSize (baseWidth, baseHeight);
    addAndMakeVisible (*root);
    if (getWidth() > 0) resized();
}

void RiffhouseEditor::applyLook (ui::Look l, bool reopenSettings)
{
    if (root != nullptr && root->wizard != nullptr) return;
    ui::setLook (l);
    lnf.applyColours();
    proc.uiState.setProperty ("look", (int) l, nullptr);
    buildRoot();
    if (reopenSettings) root->toggleOverlay (root->settings);
    repaint();
}

void RiffhouseEditor::resized()
{
    // The rig is laid out for whatever shape the window has: it is scaled so the designed size just
    // fits, and the other side grows to fill the window. Only past a very wide or very tall window
    // does the rest stay empty, centred.
    const float s = std::min ((float) getWidth() / (float) baseWidth, (float) getHeight() / (float) baseHeight);
    if (s <= 0.0f) return;
    const int w = juce::jlimit (baseWidth, maxWidth, juce::roundToInt ((float) getWidth() / s));
    const int h = juce::jlimit (baseHeight, maxHeight, juce::roundToInt ((float) getHeight() / s));
    root->setBounds (0, 0, w, h);
    root->setTransform (juce::AffineTransform::scale (s).translated (0.5f * ((float) getWidth() - (float) w * s),
                                                                      0.5f * ((float) getHeight() - (float) h * s)));
    proc.uiState.setProperty ("scale", s, nullptr);
}

void RiffhouseEditor::timerCallback()
{
    root->tick();
    if (++guardTick >= 6) { guardTick = 0; watchInputLevel(); } // five times a second
}

void RiffhouseEditor::watchInputLevel()
{
    // The standalone app looks after the input level setup chose, for as long as the player
    // has not set another by hand. In a DAW the level belongs to the session.
    if (root->wizard != nullptr) { inputGuard.reset(); return; } // setup is reading the same meter: leave it alone
    const float peak = proc.meters().stagePeak.exchange (0.0f);
    const float gain = proc.value (P::inGain);
    const juce::var looked = proc.uiState.getProperty ("guardGain");
    const bool ours = ! looked.isVoid() && std::abs ((float) looked - gain) < 0.01f;
    if (StandaloneHost::get() == nullptr || root->takeBar.isActive() || ! ours)
    {
        inputGuard.reset();
        return;
    }
    const float down = inputGuard.heard (peak, 0.2);
    if (down <= 0.0f) return;
    const float to = std::max (-24.0f, gain - down);
    proc.setValue (P::inGain, to);
    proc.uiState.setProperty ("guardGain", to, nullptr);
    proc.rememberSetup();
    showToast ("Input turned down " + juce::String (gain - to, 1).replace (".0", "") + " dB. You are playing harder than in setup.");
}

void RiffhouseEditor::showSetupWizard (bool again)
{
    auto* host = StandaloneHost::get();
    if (host == nullptr || root->wizard != nullptr) return;
    if (again)
    {
        // offered once means once, whatever happens next
        host->settings().setValue ("setupVersion", AutoSetup::kSetupVersion);
        host->settings().saveIfNeeded();
    }
    root->closeOverlays();
    root->wizard = std::make_unique<SetupWizard> (proc);
    root->addAndMakeVisible (*root->wizard);
    root->wizard->setBounds (root->getLocalBounds());
    motion::enter (*root->wizard, motion::Enter::fade);
    root->wizard->onFinished = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RiffhouseEditor> (this)]
                                         { if (safe != nullptr) safe->closeSetupWizard(); });
    };
    // listing the inputs asks every kind of driver what it has
    devlog::Timed listing ("Opening setup: listing the inputs");
    root->wizard->begin (again);
}

void RiffhouseEditor::closeSetupWizard()
{
    bool ranToTheEnd = false;
    if (root->wizard != nullptr)
    {
        ranToTheEnd = root->wizard->ranToTheEnd;
        motion::ghost (*root->wizard, motion::ui);
    }
    root->wizard.reset();
    // the one staged moment in the app: setup is done, and the rig powers on
    if (ranToTheEnd) root->powerOn();
    root->repaint();
}

void RiffhouseEditor::rehearseSetup()
{
    if (StandaloneHost::get() == nullptr) return;
    const bool wasFrozen = motion::still();
    motion::freeze (false);
    motion::freeClock();
    for (const bool onInterface : { false, true })
    {
        showSetupWizard (false);
        if (root->wizard == nullptr) break;
        AutoSetup::Summary s;
        s.deviceType = onInterface ? "ASIO" : "Windows Audio (Exclusive Mode)";
        s.inputName = onInterface ? "Focusrite USB ASIO" : "Microphone (High Definition Audio)";
        s.outputName = onInterface ? s.inputName : juce::String ("Speakers (High Definition Audio)");
        s.bufferSize = 144; s.sampleRate = 48000.0; s.latencyMs = 9.3; s.exclusive = ! onInterface;
        s.inputGainDb = 1.5f; s.inputFix = ! onInterface; s.inputFixDb = 21.0f; s.gateDb = -52.0f;
        root->wizard->finishedWith (s);
        (void) root->createComponentSnapshot (root->getLocalBounds(), true, 0.5f);
        root->wizard->startPlaying(); // the setup behind the summary is gone from here on
        (void) root->createComponentSnapshot (root->getLocalBounds(), true, 0.5f);
        closeSetupWizard();           // which draws the card once more, to let it fade
        motion::step();
        (void) root->createComponentSnapshot (root->getLocalBounds(), true, 0.5f);
    }
    motion::freeze (wasFrozen);
}

namespace
{
enum class Dropped { none, capture, tone, sound, song };

Dropped kindOf (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".nam") return Dropped::capture;
    if (ext == ".json") return Dropped::tone;
    if (SongSheetView::isSongFile (f)) return Dropped::song;
    if (juce::StringArray { ".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a" }.contains (ext)) return Dropped::sound;
    return Dropped::none;
}

// A cabinet impulse is a fraction of a second long. A song is not.
constexpr double kLongestImpulseSeconds = 4.0;
}

bool RiffhouseEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (root->wizard != nullptr) return false;
    for (const auto& f : files)
        if (kindOf (juce::File (f)) != Dropped::none) return true;
    return false;
}

void RiffhouseEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    juce::File first;
    for (const auto& f : files)
        if (kindOf (juce::File (f)) != Dropped::none) { first = juce::File (f); break; }
    juce::String line;
    switch (kindOf (first))
    {
        case Dropped::capture: line = "This capture becomes your amp."; break;
        case Dropped::tone:    line = "This tone is loaded. Save it if you want to keep it."; break;
        case Dropped::sound:
            line = root->takesView.isVisible() ? "Make an MP3 or FLAC copy of it, saved beside it."
                 : proc.audioFileSeconds (first) <= kLongestImpulseSeconds
                       ? "This impulse response becomes your cabinet."
                       : "This song plays in your headphones so you can play along. It stays out of your recording.";
            break;
        case Dropped::none: break;
    }
    root->dropHint.show ("Drop " + first.getFileName(), line);
}

void RiffhouseEditor::fileDragExit (const juce::StringArray&) { root->dropHint.setVisible (false); }

void RiffhouseEditor::filesDropped (const juce::StringArray& files, int, int)
{
    root->dropHint.setVisible (false);
    // one of each kind is enough: two amps cannot both be the amp
    bool did[4] = {};
    for (const auto& name : files)
    {
        const juce::File f (name);
        const auto kind = kindOf (f);
        if (kind == Dropped::none || did[(int) kind]) continue;
        did[(int) kind] = true;
        openFile (f);
    }
}

void RiffhouseEditor::openFile (const juce::File& f)
{
    if (! f.existsAsFile()) return;
    switch (kindOf (f))
    {
        case Dropped::capture:
        {
            root->closeOverlays();
            showToast ("Loading " + f.getFileName());
            juce::Component::SafePointer<RiffhouseEditor> safe (this);
            proc.loadCaptureAsync (f, [safe, f] (juce::String err)
            {
                if (safe == nullptr) return;
                if (err.isEmpty()) safe->root->showPage (mAmp);
                safe->root->refreshTop();
                safe->showToast (err.isEmpty() ? f.getFileNameWithoutExtension() + " is your amp now."
                                               : "Couldn't load " + f.getFileName() + ": " + err);
            });
            break;
        }
        case Dropped::tone:
        {
            Preset p;
            juce::String err;
            if (! presetFromJson (f.loadFileAsString(), p, err))
            {
                showToast (f.getFileName() + " is not a Riffhouse tone.");
                break;
            }
            if (p.name.isEmpty()) p.name = f.getFileNameWithoutExtension();
            proc.applySharedPreset (p);
            root->closeOverlays();
            root->refreshTop();
            showToast ("Loaded \"" + p.name + "\". Save it to keep it.");
            break;
        }
        case Dropped::song:
        {
            if (root->openOverlay != &root->sheetView) root->toggleOverlay (root->sheetView);
            root->sheetView.importFile (f);
            break;
        }
        case Dropped::sound:
        {
            const double seconds = proc.audioFileSeconds (f);
            if (root->takesView.isVisible() && seconds > 0.0)
            {
                root->takesView.convertDropped (f); // the list of recordings is open: this is a file to share
            }
            else if (seconds <= 0.0)
            {
                showToast ("Couldn't read " + f.getFileName() + ".");
            }
            else if (seconds <= kLongestImpulseSeconds)
            {
                if (proc.loadUserIr (f))
                {
                    root->closeOverlays();
                    root->showPage (mCab);
                    showToast (f.getFileNameWithoutExtension() + " is your cabinet now.");
                }
                else showToast ("Couldn't read " + f.getFileName() + ".");
            }
            else if (proc.loadJamTrack (f))
            {
                proc.setJamPlaying (true);
                root->refreshTop();
                showToast ("Playing " + f.getFileNameWithoutExtension() + ". Space pauses it.");
            }
            else showToast ("Couldn't read " + f.getFileName() + ".");
            break;
        }
        case Dropped::none: break;
    }
}

void RiffhouseEditor::showToast (const juce::String& m)
{
    root->toast.setTopLeftPosition (root->toast.getX(), root->bottomY() - (root->noticeBar.isVisible() ? 68 : 34));
    root->toast.toFront (false);
    root->toast.show (m);
}

void RiffhouseEditor::saveFilmstrips (const juce::File& folder)
{
    folder.createDirectory();
    auto& r = *root;
    r.wizard.reset();
    r.quietNotices = true;
    r.demoLevels = true;
    r.noticeBar.set ({});
    motion::freeze (false);
    motion::setReduced (false);
    double clock = 10000.0;
    motion::setClock (clock);
    // `act` happens at time 0; the strip shows the area at each of the given milliseconds after it
    auto film = [&] (const juce::String& name, juce::Rectangle<int> area, std::function<void()> act,
                     std::initializer_list<int> times = { 0, 30, 60, 100, 150, 200, 260, 330 }, float scale = 1.0f)
    {
        clock += 5000.0; // whatever was in flight is long over
        motion::setClock (clock);
        motion::step();
        r.tick();
        // paint, let whatever that painting set off run out, and paint again: every glide is at rest where it starts
        (void) r.createComponentSnapshot (area, true, scale);
        clock += 2000.0;
        motion::setClock (clock);
        motion::step();
        (void) r.createComponentSnapshot (area, true, scale);
        act();
        const int w = juce::roundToInt ((float) area.getWidth() * scale), h = juce::roundToInt ((float) area.getHeight() * scale);
        juce::Image strip (juce::Image::RGB, (w + 6) * (int) times.size() - 6, h + 22, true);
        juce::Graphics sg (strip);
        sg.fillAll (juce::Colour (0xff2a2a2a));
        int x = 0;
        for (int ms : times)
        {
            motion::setClock (clock + (double) ms);
            motion::step();
            const auto img = r.createComponentSnapshot (area, true, scale);
            sg.drawImageAt (img, x, 22);
            sg.setColour (juce::Colours::white);
            sg.setFont (font (13.0f));
            sg.drawText (juce::String (ms) + " ms", x + 4, 2, 100, 18, juce::Justification::centredLeft);
            x += w + 6;
        }
        clock += 1000.0;
        motion::setClock (clock);
        motion::step();
        juce::PNGImageFormat png;
        auto f = folder.getChildFile (name + ".png");
        f.deleteFile();
        if (auto out = f.createOutputStream()) png.writeImageToStream (strip, *out);
    };

    const auto whole = r.getLocalBounds();
    const auto strip = r.chain.getBounds();
    r.showPage (mAmp);
    film ("page-switch", strip.getUnion (r.amp.getBounds().withHeight (260)).withTrimmedRight (whole.getWidth() / 2),
          [&] { r.showPage (mDrive); }, { 0, 30, 60, 100, 150, 200, 260, 330 }, 0.6f);
    r.showPage (mDrive);
    const float driveWas = proc.value (P::drvOn);
    proc.setValue (P::drvOn, 1.0f);
    film ("pedal-off", r.amp.getBounds().withSizeKeepingCentre (300, r.amp.getHeight()).getUnion (r.chain.getBounds().withWidth (whole.getWidth() / 2)).withTrimmedLeft (380),
          [&] { proc.setValue (P::drvOn, 0.0f); }, { 0, 30, 60, 100, 150, 200, 260 }, 0.6f);
    film ("pedal-on", r.amp.getBounds().withSizeKeepingCentre (300, r.amp.getHeight()).getUnion (r.chain.getBounds().withWidth (whole.getWidth() / 2)).withTrimmedLeft (380),
          [&] { proc.setValue (P::drvOn, 1.0f); }, { 0, 20, 40, 60, 100, 150, 200 }, 0.6f);
    proc.setValue (P::drvOn, driveWas);
    r.showPage (mAmp);
    const float ampWas = proc.value (P::ampOn);
    proc.setValue (P::ampOn, 0.0f);
    film ("amp-on", r.amp.getBounds().withTrimmedLeft (r.amp.getWidth() / 2), [&] { proc.setValue (P::ampOn, 1.0f); },
          { 0, 40, 80, 120, 180, 240, 300 }, 0.5f);
    film ("amp-off", r.amp.getBounds().withTrimmedLeft (r.amp.getWidth() / 2), [&] { proc.setValue (P::ampOn, 0.0f); },
          { 0, 60, 120, 200, 300, 420, 480 }, 0.5f);
    proc.setValue (P::ampOn, ampWas);
    film ("settings-open", whole, [&] { r.toggleOverlay (r.settings); }, { 0, 40, 80, 120, 180, 240, 300 }, 0.35f);
    film ("settings-close", whole, [&] { r.closeOverlays(); }, { 0, 30, 60, 100, 150, 200 }, 0.35f);
    film ("popover-open", whole.withTrimmedLeft (whole.getWidth() / 2).withHeight (420),
          [&] { r.showPopover (r.recordPanel, r.recordOptions.getBounds().getUnion (r.record.getBounds()), 420, 560); }, { 0, 40, 80, 120, 180, 240, 300 }, 0.5f);
    film ("popover-close", whole.withTrimmedLeft (whole.getWidth() / 2).withHeight (420), [&] { r.closePopovers(); }, { 0, 30, 60, 100, 150, 200 }, 0.5f);
    film ("tone-step", strip.getUnion (r.amp.getBounds()), [&] { r.stepPreset (1); }, { 0, 30, 60, 100, 150, 200 }, 0.35f);
    r.stepPreset (-1);
    film ("toast", whole.withTop (whole.getBottom() - 150).withSizeKeepingCentre (700, 150), [&] { showToast ("Saved. Press L to listen to your takes."); },
          { 0, 40, 80, 120, 200, 2400, 2540, 2600, 2700 }, 0.6f);
    r.toast.setVisible (false);
    {
        // a knob that undo puts back travels there
        auto& b = r.saveButton;
        film ("button-press", b.getBounds().expanded (24), [&] { b.setState (juce::Button::buttonDown); }, { 0, 20, 40, 60, 90, 120 }, 2.0f);
        film ("button-release", b.getBounds().expanded (24), [&] { b.setState (juce::Button::buttonNormal); }, { 0, 30, 60, 100, 150, 200 }, 2.0f);
    }
    {
        const float was = proc.value (P::outLevel);
        proc.historyTick (false);
        film ("knob-undo", r.volume.getBounds().expanded (12),
              [&]
              {
                  proc.setValue (P::outLevel, was > -6.0f ? was - 14.0f : was + 14.0f);
                  motion::announceJump();
                  proc.setValue (P::outLevel, was);
              }, { 0, 40, 80, 130, 180, 230, 260, 300 }, 2.0f);
        proc.setValue (P::outLevel, was);
    }
    {
        // setup: a step finishes, the summary arrives, and the rig powers on behind it
        r.showPage (mAmp);
        r.wizard = std::make_unique<SetupWizard> (proc);
        auto& w = *r.wizard;
        r.addAndMakeVisible (w);
        w.setBounds (whole);
        w.showExample (0);
        const auto card = whole.withSizeKeepingCentre (700, 600);
        film ("setup-step-done", card, [&] { w.showExample (4, true); }, { 0, 40, 80, 120, 180, 240, 300 }, 0.5f);
        w.showExample (3);
        film ("setup-summary", card, [&] { w.showExample (5, true); }, { 0, 60, 120, 180, 240, 320, 400, 500, 600, 700 }, 0.5f);
        film ("setup-power-on", whole.withHeight (r.amp.getY() + 330),
              [&]
              {
                  motion::ghost (w, motion::ui);
                  r.wizard.reset();
                  r.powerOn();
              }, { 0, 60, 130, 200, 280, 360, 450, 560, 700 }, 0.5f);
    }
    motion::freeClock();
    motion::freeze (true);
}

juce::String RiffhouseEditor::saveFilm (const juce::File& folder, const juce::File& input, const juce::var& script,
                                       int fps, float scale, double seconds)
{
    // the recording
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (input));
    if (reader == nullptr) return "Can't read " + input.getFullPathName();
    const double sr = reader->sampleRate;
    const int length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (sr * 600.0));
    juce::AudioBuffer<float> di (2, std::max (1, length));
    di.clear();
    reader->read (&di, 0, length, 0, true, true); // a mono file lands on both sides, which the input reads as one guitar
    const int total = seconds > 0.0 ? juce::roundToInt (seconds * sr) : length;
    if (total <= 0) return "The recording is empty";
    fps = juce::jlimit (1, 120, fps);

    // the script: {"steps": [...]} or the list itself
    struct Step
    {
        double t = 0.0, over = 0.0;
        juce::String tone;
        int page = -1;
        int param = -1;
        float to = 0.0f;  // where the knob goes, 0 to 1 along its travel
        float from = -1.0f;
        bool started = false, finished = false;
        // the pointer: glide to a control or a chain pedal, click one, or hold a knob while it turns
        juce::String pointTo, clickOn;
        bool knobPointer = false, clicked = false;
    };
    std::vector<Step> steps;
    const juce::var list = script.isArray() ? script : script.getProperty ("steps", juce::var());
    if (list.isArray())
        for (const auto& v : *list.getArray())
        {
            Step st;
            st.t = (double) v.getProperty ("t", 0.0);
            st.over = std::max (0.0, (double) v.getProperty ("over", 0.0));
            if (v.hasProperty ("tone"))
            {
                st.tone = v["tone"].toString();
                if (proc.presets().indexOf (st.tone) < 0) return "No tone called \"" + st.tone + "\"";
            }
            if (v.hasProperty ("page"))
            {
                const auto id = v["page"].toString();
                for (int i = 0; i < numModules; ++i)
                    if (id.equalsIgnoreCase (modules()[(size_t) i].id)) st.page = i;
                if (st.page < 0) return "No page called \"" + id + "\"";
            }
            auto known = [&] (const juce::String& id)
            {
                for (const auto& m : modules())
                    if (id.equalsIgnoreCase (m.id)) return true;
                return findParamByKey (id.toStdString()) >= 0;
            };
            for (auto* key : { "pointer", "click" })
                if (v.hasProperty (key) && ! v[key].isBool())
                {
                    const auto id = v[key].toString();
                    if (! known (id)) return "Nothing called \"" + id + "\" to point at (a page like drive, or a control like drv_on)";
                    (juce::String (key) == "pointer" ? st.pointTo : st.clickOn) = id;
                }
            st.knobPointer = v.hasProperty ("pointer") && v["pointer"].isBool() && (bool) v["pointer"];
            const auto knob = v.getProperty ("knob", v.getProperty ("param", juce::var())).toString();
            if (knob.isNotEmpty())
            {
                for (const auto& sp : paramSpecs())
                    if (knob == sp.key) st.param = (int) sp.id;
                if (st.param < 0) return "No control called \"" + knob + "\" (use the names in a tone file, like drv_gain)";
                if (v.hasProperty ("value")) st.to = proc.param ((P) st.param)->convertTo0to1 ((float) v["value"]);
                else st.to = juce::jlimit (0.0f, 1.0f, (float) v.getProperty ("to", 0.5));
            }
            steps.push_back (st);
        }
    std::stable_sort (steps.begin(), steps.end(), [] (const Step& a, const Step& b) { return a.t < b.t; });

    folder.createDirectory();
    for (const auto& old : folder.findChildFiles (juce::File::findFiles, false, "frame_*.png")) old.deleteFile();
    stopTimer(); // the window is ticked once a frame from here, as its timer would at 30 a second
    // the real mouse (wherever the system left it) never shows a tooltip in a film
    tooltips.setMillisecondsBeforeTipAppears (1 << 30);
    tooltips.hideTip();
    auto& r = *root;
    r.wizard.reset();
    r.quietNotices = true;
    r.demoLevels = false;
    r.noFigures = true;
    r.noticeBar.set ({});
    r.closeOverlays();
    motion::freeze (false);
    motion::setReduced (false);
    const double clock0 = 100000.0; // frame 0; the unseen settling second runs just before it
    motion::setClock (clock0);

    const int block = 128;
    proc.releaseResources();
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> out (2, total), work (2, block);
    juce::MidiBuffer midi;
    float inMax = 0.0f, outMax = 0.0f;
    int doneSamples = 0;

    // The pointer, in the window's coordinates: a glide from one place to the next on a slight
    // curve, the way a hand moves a mouse. On a knob that turns, it moves up and down with it,
    // as dragging a knob does.
    struct Pointer
    {
        bool shown = false;
        juce::Point<float> at, from, to;
        double t0 = 0.0, t1 = 0.0;
        juce::Component::SafePointer<juce::Component> target; // a tone change can rebuild a page under it
        int dragParam = -1;
        float dragFrom = 0.0f;
        double pressFrom = 1.0e9, pressTo = -1.0;
    } ptr;
    juce::String pointerError;
    std::function<void (juce::Component&, const juce::String&, juce::Component*&, float&)> walk;
    walk = [&] (juce::Component& c, const juce::String& id, juce::Component*& best, float& area)
    {
        for (auto* ch : c.getChildren())
        {
            if (! ch->isVisible()) continue;
            const auto& pr = ch->getProperties();
            if ((pr["page"].toString().isNotEmpty() && pr["page"].toString().equalsIgnoreCase (id)) || pr["control"].toString() == id)
                if (ch->isShowing() && (float) (ch->getWidth() * ch->getHeight()) > area) { best = ch; area = (float) (ch->getWidth() * ch->getHeight()); }
            walk (*ch, id, best, area);
        }
    };
    auto findTarget = [&] (const juce::String& id, double t) -> juce::Component*
    {
        juce::Component* best = nullptr;
        float area = 0.0f;
        walk (r, id, best, area);
        if (best == nullptr && pointerError.isEmpty())
            pointerError = "\"" + id + "\" is not on screen at " + juce::String (t, 2) + " s (show its page first)";
        return best;
    };
    auto aimAt = [&] (juce::Component& c)
    {
        // a knob is held by its cap, a little right of and below the middle; anything else in its middle
        juce::Component* spot = &c;
        for (auto* ch : c.getChildren())
            if (dynamic_cast<juce::Slider*> (ch) != nullptr) spot = ch;
        auto b = spot->getLocalBounds().toFloat();
        auto p = b.getCentre();
        if (spot != &c) p += { b.getWidth() * 0.1f, b.getHeight() * 0.12f };
        else if (c.getProperties()["page"].toString().isNotEmpty()) p.y = b.getY() + b.getHeight() * 0.4f; // a chain pedal by its picture, not its name
        return getLocalPoint (spot, p);
    };
    auto glideTo = [&] (juce::Component* target, double t, double seconds)
    {
        if (target == nullptr) return;
        const auto to = aimAt (*target);
        if (! ptr.shown)
        {
            // it comes in from below and to the right, as a hand reaching for the mouse
            ptr.at = { juce::jmin ((float) getWidth() - 4.0f, to.x + getWidth() * 0.16f), juce::jmin ((float) getHeight() - 4.0f, to.y + getHeight() * 0.14f) };
            ptr.shown = true;
        }
        ptr.from = ptr.at;
        ptr.to = to;
        ptr.t0 = t;
        ptr.t1 = t + (ptr.from.getDistanceFrom (to) < 3.0f ? 0.0 : seconds);
        ptr.target = target;
        ptr.dragParam = -1;
        motion::filmPointer = nullptr; // it rests on nothing while it moves
    };
    auto pointerAt = [&] (double t)
    {
        if (! ptr.shown) return;
        // it follows its control when the layout moves (a tone change swaps the amp's face)
        if (ptr.target != nullptr && ptr.target->isShowing()) ptr.to = aimAt (*ptr.target);
        const float k = ptr.t1 <= ptr.t0 ? 1.0f : (float) juce::jlimit (0.0, 1.0, (t - ptr.t0) / (ptr.t1 - ptr.t0));
        const float e = motion::ease (motion::Curve::inOut, k);
        const auto d = ptr.to - ptr.from;
        const juce::Point<float> bend { d.y * 0.12f, -d.x * 0.12f }; // a slight arc, never a ruler line
        const auto mid = (ptr.from + ptr.to) * 0.5f + bend;
        ptr.at = ptr.from * ((1 - e) * (1 - e)) + mid * (2 * (1 - e) * e) + ptr.to * (e * e);
        if (k >= 1.0f) motion::filmPointer = ptr.target.getComponent();
        if (ptr.dragParam >= 0 && k >= 1.0f)
        {
            // dragging a knob up turns it up: the hand moves with the value
            const float moved = proc.param ((P) ptr.dragParam)->getValue() - ptr.dragFrom;
            ptr.at = ptr.to - juce::Point<float> (0.0f, moved * 70.0f * std::max (0.25f, r.getTransform().getScaleFactor()));
        }
        motion::filmPress = t >= ptr.pressFrom && t < ptr.pressTo;
    };

    auto takeSteps = [&] (double t)
    {
        for (auto& st : steps)
        {
            if (st.t > t) break;
            if (! st.started)
            {
                st.started = true;
                if (st.tone.isNotEmpty()) proc.loadPreset (proc.presets().indexOf (st.tone));
                if (st.page >= 0) r.showPage (st.page);
                if (st.param >= 0) st.from = proc.param ((P) st.param)->getValue();
                if (st.pointTo.isNotEmpty()) glideTo (findTarget (st.pointTo, st.t), st.t, st.over > 0.0 ? st.over : 0.6);
                if (st.clickOn.isNotEmpty())
                {
                    glideTo (findTarget (st.clickOn, st.t), st.t, st.over > 0.0 ? st.over : 0.4);
                    ptr.pressFrom = ptr.t1 + 0.05; // a beat to land, then the press, three frames at 30 a second
                    ptr.pressTo = ptr.pressFrom + 0.1;
                }
                if (st.knobPointer && st.param >= 0)
                {
                    auto* knob = findTarget (paramSpecs()[(size_t) st.param].key, st.t);
                    if (knob != nullptr && ptr.target.getComponent() != knob) glideTo (knob, st.t, 0.25);
                    ptr.target = knob;
                    ptr.dragParam = st.param;
                    ptr.dragFrom = st.from;
                    ptr.pressFrom = st.t;
                    ptr.pressTo = st.t + std::max (0.1, st.over) + 0.05;
                }
            }
            if (st.clickOn.isNotEmpty() && ! st.clicked && t >= ptr.pressTo && ptr.target != nullptr)
            {
                // the press lets go: a chain pedal opens its page, a switch flips
                st.clicked = true;
                const auto page = ptr.target->getProperties()["page"].toString();
                if (auto* b = dynamic_cast<juce::Button*> (ptr.target.getComponent()); b != nullptr && b->onClick)
                    b->onClick(); // an arrow beside a chooser: the next choice
                else if (page.isNotEmpty())
                {
                    for (int i = 0; i < numModules; ++i)
                        if (page.equalsIgnoreCase (modules()[(size_t) i].id)) r.showPage (i);
                }
                else if (const int id = findParamByKey (st.clickOn.toStdString()); id >= 0)
                {
                    auto* p = proc.param ((P) id);
                    p->setValueNotifyingHost (p->getValue() >= 0.5f ? 0.0f : 1.0f);
                }
            }
            if (st.param >= 0 && ! st.finished)
            {
                // a move ends where it was sent, once: after that the knob is free again for a new tone or a new move
                const float k = st.over <= 0.0 ? 1.0f : (float) juce::jlimit (0.0, 1.0, (t - st.t) / st.over);
                if (k >= 1.0f) st.finished = true;
                const float eased = motion::ease (motion::Curve::inOut, k);
                auto* p = proc.param ((P) st.param);
                const float v = st.from + (st.to - st.from) * eased;
                if (std::abs (p->getValue() - v) > 1.0e-6f) p->setValueNotifyingHost (v);
            }
        }
    };

    // The film starts settled on its first state: the steps at time 0 are taken, then a second of
    // quiet runs through the rig and the window (tone change dip, lamps, auto level), unseen.
    {
        for (auto& st : steps)
            if (st.t <= 0.0 && st.param < 0) { st.started = true; if (st.tone.isNotEmpty()) proc.loadPreset (proc.presets().indexOf (st.tone)); if (st.page >= 0) r.showPage (st.page); }
        juce::AudioBuffer<float> quiet (2, block);
        for (int i = 0; i < juce::roundToInt (sr / block); ++i)
        {
            quiet.clear();
            midi.clear();
            proc.processBlock (quiet, midi);
        }
        for (int i = 0; i < 30; ++i)
        {
            motion::setClock (clock0 - 1000.0 + i * (1000.0 / 30.0));
            motion::step();
            r.tick();
        }
        (void) createComponentSnapshot (getLocalBounds(), true, 1.0f); // whatever that painting starts is over by frame 0
        motion::setClock (clock0 - 1.0);
        motion::step();
        proc.meters().gateInPeak.store (0.0f);
        proc.meters().outPeak.store (0.0f);
    }

    const int frames = (int) std::ceil ((double) total * fps / sr);
    juce::PNGImageFormat png;
    for (int f = 0; f < frames; ++f)
    {
        const double t = (double) f / fps;
        const int until = std::min (total, juce::roundToInt (t * sr));
        while (doneSamples < until)
        {
            takeSteps ((double) doneSamples / sr);
            const int n = std::min (block, until - doneSamples);
            work.setSize (2, n, false, false, true);
            for (int c = 0; c < 2; ++c)
            {
                if (doneSamples + n <= length) work.copyFrom (c, 0, di, c, doneSamples, n);
                else
                {
                    work.clear (c, 0, n);
                    if (doneSamples < length) work.copyFrom (c, 0, di, c, doneSamples, length - doneSamples);
                }
            }
            midi.clear();
            proc.processBlock (work, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, doneSamples, work, c, 0, n);
            inMax = std::max (inMax, proc.meters().gateInPeak.load());
            outMax = std::max (outMax, proc.meters().outPeak.load());
            doneSamples += n;
        }
        takeSteps (t);
        // the meters read what this frame's audio did, the way the window reads the last block
        if (f > 0)
        {
            proc.meters().gateInPeak.store (inMax);
            proc.meters().outPeak.store (outMax);
        }
        inMax = outMax = 0.0f;
        motion::setClock (clock0 + t * 1000.0);
        motion::step();
        r.tick();
        pointerAt (t);
        if (pointerError.isNotEmpty()) break;
        auto img = createComponentSnapshot (getLocalBounds(), true, scale);
        if (ptr.shown)
        {
            // the arrow Windows draws, at its size beside this window's controls, sharp at any film scale
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (scale));
            const float u = 1.2f * std::max (0.25f, r.getTransform().getScaleFactor());
            juce::Path arrow;
            arrow.startNewSubPath (0.0f, 0.0f);
            arrow.lineTo (0.0f, 16.0f);
            arrow.lineTo (3.7f, 12.6f);
            arrow.lineTo (6.3f, 18.5f);
            arrow.lineTo (8.7f, 17.5f);
            arrow.lineTo (6.1f, 11.6f);
            arrow.lineTo (11.0f, 11.6f);
            arrow.closeSubPath();
            arrow.applyTransform (juce::AffineTransform::scale (u).translated (ptr.at));
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillPath (arrow, juce::AffineTransform::translation (0.8f * u, 1.4f * u));
            g.setColour (juce::Colours::white);
            g.fillPath (arrow);
            g.setColour (juce::Colours::black);
            g.strokePath (arrow, juce::PathStrokeType (1.0f * u, juce::PathStrokeType::mitered));
        }
        auto file = folder.getChildFile ("frame_" + juce::String (f).paddedLeft ('0', 5) + ".png");
        if (auto stream = file.createOutputStream()) png.writeImageToStream (img, *stream);
        else return "Can't write " + file.getFullPathName();
    }
    motion::filmPointer = nullptr;
    motion::filmPress = false;
    if (pointerError.isNotEmpty()) return pointerError;
    // whatever is left after the last frame, so the sound runs to the end of the recording
    while (doneSamples < total)
    {
        takeSteps ((double) doneSamples / sr);
        const int n = std::min (block, total - doneSamples);
        work.setSize (2, n, false, false, true);
        for (int c = 0; c < 2; ++c)
        {
            work.clear (c, 0, n);
            if (doneSamples < length) work.copyFrom (c, 0, di, c, doneSamples, std::min (n, length - doneSamples));
        }
        midi.clear();
        proc.processBlock (work, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, doneSamples, work, c, 0, n);
        doneSamples += n;
    }
    auto wav = folder.getChildFile ("audio.wav");
    wav.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream (wav.createOutputStream());
    if (stream == nullptr) return "Can't write " + wav.getFullPathName();
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), sr, 2, 24, {}, 0));
    if (writer == nullptr) return "Can't write " + wav.getFullPathName();
    stream.release(); // the writer owns it now
    writer->writeFromAudioSampleBuffer (out, 0, total);
    writer.reset();
    folder.getChildFile ("film.txt").replaceWithText (juce::String (frames) + " frames at " + juce::String (fps) + " fps, "
                                                     + juce::String (getWidth()) + "x" + juce::String (getHeight()) + " at " + juce::String (scale)
                                                     + "x, audio.wav " + juce::String (sr, 0) + " Hz, " + juce::String ((double) total / sr, 2) + " s\n");
    return {};
}

void RiffhouseEditor::saveSnapshots (const juce::File& folder, float scale, float cutouts)
{
    folder.createDirectory();
    motion::freeze (true); // pictures show finished states, never something half way
    auto& r = *root;
    r.wizard.reset();
    r.quietNotices = true;
    r.demoLevels = true;
    r.noticeBar.set ({});
    auto shoot = [&] (const juce::String& name)
    {
        r.tick();
        const auto img = r.createComponentSnapshot (r.getLocalBounds(), true, scale);
        juce::PNGImageFormat png;
        auto f = folder.getChildFile (name + ".png");
        f.deleteFile();
        if (auto out = f.createOutputStream()) png.writeImageToStream (img, *out);
    };
    {
        // the app icon, full size and the plainer one for small sizes
        juce::PNGImageFormat png;
        for (auto [name, size, simple] : { std::tuple<const char*, int, bool> { "icon.png", 1024, false }, { "icon-small.png", 128, true } })
        {
            auto f = folder.getChildFile (name);
            f.deleteFile();
            if (auto out = f.createOutputStream()) png.writeImageToStream (renderAppIcon (size, simple), *out);
        }
    }
    r.closeOverlays();
    for (int i = 0; i < numModules; ++i)
    {
        r.showPage (i);
        shoot (juce::String (i).paddedLeft ('0', 2) + "-" + modules()[(size_t) i].id);
    }
    r.showPage (mAmp);
    shoot ("main");
    {
        // one picture with the message that shows when no guitar is connected
        r.noticeBar.set ({ "No guitar input yet. Plug in and Riffhouse will find it.", "Run setup", false });
        r.noticeBar.setBounds (0, r.bottomY() - 34, r.getWidth(), 34);
        shoot ("notice");
        r.noticeBar.set ({});
    }
    {
        // teaching a footswitch, the way a player does it: flip the drive pedal's switch, then press the switch on the floor
        const float driveWas = proc.value (P::drvOn);
        const auto listWas = proc.footControl().toText();
        proc.footTeachTouch();
        r.footStrip = true;
        r.noticeBar.set (r.footNotice());
        r.noticeBar.setBounds (0, r.bottomY() - 34, r.getWidth(), 34);
        shoot ("foot-touch");
        proc.setValue (P::drvOn, driveWas > 0.5f ? 0.0f : 1.0f);
        proc.footTick();
        r.noticeBar.set (r.footNotice());
        shoot ("foot-move");
        proc.footMoved (20, 127); // the press: it is taught, and the pedal goes on
        const bool onAfterPress = proc.value (P::drvOn) > 0.5f;
        proc.footMoved (20, 0);   // a quick release: this switch springs back
        proc.footMoved (20, 127); // so the next press flips the pedal off
        std::printf ("foot control: %s; taught=%d on-after-press=%d off-after-next=%d\n", proc.footControl().toText().c_str(),
                     (int) (proc.footTeaching() == RiffhouseProcessor::FootTeach::off), (int) onAfterPress, (int) (proc.value (P::drvOn) < 0.5f));
        shoot ("foot-taught");
        // and everything back as it was
        for (size_t i = proc.footControl().bindings().size(); i-- > 0;)
            if (proc.footControl().bindings()[i].controller == 20) proc.footRemove (i);
        proc.setValue (P::drvOn, driveWas);
        std::printf ("foot control restored: %d\n", (int) (proc.footControl().toText() == listWas));
        r.toast.setVisible (false);
    }
    {
        // the takes list with one take playing, then the rig with a take running through it
        r.toggleOverlay (r.takesView);
        const auto takes = listTakes (proc.recordingFolder);
        if (takes.size() > 1) r.takeBar.start (takes[1], false);
        r.takesView.refresh();
        shoot ("takes");
        r.closeOverlays();
        if (takes.size() > 1 && takes[1].hasClean())
        {
            r.takeBar.start (takes[1], true);
            shoot ("take-tone");
        }
        r.takeBar.stop();
    }
    r.toggleOverlay (r.browser);
    shoot ("browser");
    r.browser.show ("Inspired By", {});
    shoot ("browser-inspired");
    r.browser.show ("All", "metallica");
    shoot ("browser-band");
    r.browser.show ("All", {});
    r.closeOverlays();
    r.toggleOverlay (r.tuner);
    shoot ("tuner-idle");
    {
        // feed the tuner an A string a few cents sharp so the picture shows a reading
        const double sr = 48000.0, hz = 110.0 * std::pow (2.0, 6.0 / 1200.0);
        std::vector<float> buf (2400);
        double ph = 0.0;
        for (int block = 0; block < 40; ++block)
        {
            for (auto& x : buf)
            {
                x = 0.3f * (float) (std::sin (ph) + 0.4 * std::sin (2.0 * ph) + 0.2 * std::sin (3.0 * ph));
                ph += juce::MathConstants<double>::twoPi * hz / sr;
            }
            proc.tuner().push (buf.data(), (int) buf.size());
            if (block > 20) r.tuner.refresh();
        }
    }
    shoot ("tuner");
    r.closeOverlays();
    r.toggleOverlay (r.settings);
    shoot ("settings-none");
    r.settings.showExampleCard();
    shoot ("settings");
    {
        using S = SettingsView::Section;
        const std::pair<S, const char*> each[] = { { S::guitar, "settings-guitar" }, { S::foot, "settings-foot" },
                                                    { S::app, "settings-app" }, { S::help, "settings-help" } };
        for (const auto& [section, name] : each) { r.settings.show (section); shoot (name); }
        r.settings.show (S::sound);
    }
    r.closeOverlays();
    r.toggleOverlay (r.sheetView);
    shoot ("sheet-empty");
    r.sheetView.showExample (0, false);
    shoot ("sheet");
    r.sheetView.showExample (2, false);
    shoot ("sheet-key");
    r.sheetView.showExample (0, false, 1.0f);
    shoot ("sheet-tab");
    r.sheetView.showExample (0, true);
    shoot ("sheet-edit");
    r.closeOverlays();
    r.showPopover (r.recordPanel, r.recordOptions.getBounds(), 380, 522);
    shoot ("record");
    {
        // the same window with a song in it, slowed down, one part going round
        const auto song = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Slow Blues in A.wav");
        song.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::FileOutputStream> stream (song.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> writer;
            if (stream != nullptr) writer.reset (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));
            if (writer != nullptr)
            {
                stream.release(); // the writer owns it now
                juce::AudioBuffer<float> tone (1, 44100);
                for (int i = 0; i < tone.getNumSamples(); ++i) tone.setSample (0, i, 0.1f * std::sin ((float) i * 0.0313f));
                for (int second = 0; second < 204; ++second) writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
            }
        }
        if (proc.loadJamTrack (song))
        {
            proc.setJamSpeed (0.8f);
            proc.setJamSection (52.0, 71.0);
            proc.setJamPlaying (true);
            r.recordPanel.refresh();
            r.recordPanel.repaint();
            shoot ("record-song");
            proc.setJamPlaying (false);
            proc.clearJamSection();
            proc.setJamSpeed (1.0f);
            r.recordPanel.refresh();
        }
        song.deleteFile();
    }
    r.closePopovers();
    r.showPopover (r.metroPanel, r.metroButton.getBounds(), 300, 274);
    shoot ("metronome");
    r.closePopovers();
    r.showPopover (r.looperPanel, r.loopButton.getBounds(), 320, 286);
    shoot ("looper");
    r.closePopovers();
    r.showPopover (r.saveDialog, r.saveButton.getBounds(), 360, 206);
    shoot ("save");
    r.closePopovers();
    {
        SetupWizard w (proc);
        r.addAndMakeVisible (w);
        w.setBounds (r.getLocalBounds());
        const std::vector<AutoSetup::Input> pretend { { "ASIO", "USB Audio Interface ASIO" },
                                                      { "Windows Audio", "Microphone (High Definition Audio)" },
                                                      { "Windows Audio", "Analogue 1 + 2 (USB Audio Interface)" } };
        w.begin (false, &pretend);
        shoot ("setup");
        w.begin (true, &pretend);
        shoot ("setup-again");
        const char* names[] = { "setup-1-find", "setup-2-delay", "setup-3-play", "setup-4-quiet", "setup-found", "setup-done" };
        for (int i = 0; i < 6; ++i) { w.showExample (i); shoot (names[i]); }
        w.showExample (6);
        shoot ("setup-done-shared");
        w.showExample (7);
        shoot ("setup-sound");
        r.removeChildComponent (&w);
    }
    {
        // the audio settings by hand: an input, an output, a delay and one switch
        AudioPicker a (proc);
        a.showExample();
        r.addAndMakeVisible (a);
        a.setCentrePosition (r.getLocalBounds().getCentre());
        shoot ("audio-by-hand");
        a.showExample (true);
        shoot ("audio-asio");
        a.showProblemExample();
        a.setCentrePosition (r.getLocalBounds().getCentre());
        shoot ("audio-rates-differ");
        r.removeChildComponent (&a);
    }

    // every amp and every drive pedal, as drawn gear
    {
        const float amp0 = proc.value (P::ampModel), drv0 = proc.value (P::drvType), drvOn0 = proc.value (P::drvOn);
        r.closeOverlays();
        r.showPage (mAmp);
        for (int m = 0; m < choices::numAmpModels; ++m)
        {
            proc.setValue (P::ampModel, (float) m);
            shoot ("gear-amp-" + juce::String (m).paddedLeft ('0', 2));
        }
        proc.setValue (P::ampModel, amp0);
        r.showPage (mDrive);
        proc.setValue (P::drvOn, 1.0f);
        for (int t = 0; t < 7; ++t)
        {
            proc.setValue (P::drvType, (float) t);
            shoot ("gear-drive-" + juce::String (t));
        }
        proc.setValue (P::drvType, drv0);
        proc.setValue (P::drvOn, drvOn0);

        // every type of the other gear with more than one look
        struct Set { int page; P type, on; int n; const char* name; };
        for (auto set : { Set { mMod, P::modType, P::modOn, 6, "mod" }, Set { mDelay, P::dlyType, P::dlyOn, 4, "delay" },
                          Set { mReverb, P::revType, P::revOn, 6, "reverb" }, Set { mCab, P::cabModel, P::cabOn, 9, "cab" } })
        {
            const float t0 = proc.value (set.type), on0 = proc.value (set.on);
            r.showPage (set.page);
            proc.setValue (set.on, 1.0f);
            for (int t = 0; t < set.n; ++t)
            {
                proc.setValue (set.type, (float) t);
                shoot (juce::String ("gear-") + set.name + "-" + juce::String (t));
            }
            proc.setValue (set.type, t0);
            proc.setValue (set.on, on0);
        }
    }

    {
        // a file held over the window, then a tone file dropped on it
        r.showPage (mAmp);
        r.dropHint.show ("Drop Midnight Lead.json", "This tone is loaded. Save it if you want to keep it.");
        shoot ("drop");
        r.dropHint.setVisible (false);
        auto tone = proc.captureCurrent ("Dropped Tone", "Shared");
        tone.values[(size_t) P::ampModel] = (float) choices::britPlexi;
        const auto file = folder.getChildFile ("dropped-tone.json");
        file.replaceWithText (presetToJson (tone));
        openFile (file);
        shoot ("dropped-tone");
        file.deleteFile();
    }

    // each piece of gear on its own with a clear background, for the website and the README
    {
        const auto cut = folder.getChildFile ("cutouts");
        cut.createDirectory();
        juce::PNGImageFormat png;
        auto write = [&] (const juce::String& name, const juce::Image& img)
        {
            auto f = cut.getChildFile (name + ".png");
            f.deleteFile();
            if (auto out = f.createOutputStream()) png.writeImageToStream (img, *out);
        };
        const float big = 3.0f * cutouts;
        struct Family { int module; int types; const char* name; };
        for (auto fam : { Family { mGate, 1, "gate" }, Family { mComp, 1, "comp" }, Family { mWah, 3, "wah" }, Family { mPitch, 1, "pitch" },
                          Family { mDrive, 7, "drive" }, Family { mEq, 1, "eq" }, Family { mMod, 6, "mod" }, Family { mDelay, 4, "delay" },
                          Family { mReverb, 6, "reverb" }, Family { mLofi, 1, "lofi" } })
            for (int t = 0; t < fam.types; ++t)
            {
                const auto& d = *pedalDesignFor (fam.module, t);
                const bool faders = fam.module == mEq;
                juce::StringArray names;
                for (auto k : modules()[(size_t) fam.module].knobs) names.add (spec (k).name);
                auto thumb = pedalThumb (d, faders ? 8 : names.size(), faders, big, faders ? nullptr : &names);
                juce::Graphics tg (thumb.image);
                tg.addTransform (juce::AffineTransform::translation (-thumb.area.getX(), -thumb.area.getY()).scaled (big));
                drawPedalLed (tg, thumb.pedal, d, true);
                write (juce::String (fam.name) + "-" + juce::String (t), thumb.image);
            }
        for (int m = 0; m < choices::numAmpModels; ++m)
        {
            const auto& d = ampDesign (m);
            juce::StringArray names;
            for (auto k : { P::ampGain, P::ampBass, P::ampMid, P::ampTreble, P::ampPresence, P::ampDepth, P::ampMaster })
                names.add (spec (k).name);
            names.add ("Volume");
            auto thumb = ampThumb (d, m == choices::neuralCapture ? juce::String ("your capture") : juce::String (spec (P::ampModel).choices[(size_t) m]).toLowerCase(), 1.6f * cutouts, &names);
            juce::Graphics tg (thumb.image);
            tg.addTransform (juce::AffineTransform::translation (-thumb.area.getX(), -thumb.area.getY()).scaled (1.6f * cutouts));
            drawAmpJewel (tg, thumb.amp, d, true);
            write ("amp-" + juce::String (m).paddedLeft ('0', 2), thumb.image);
        }
        for (int m = 0; m < 8; ++m)
            write ("cab-" + juce::String (m), cabThumb (m, 2.0f * cutouts).image);
    }

    // what the 3D renderer needs to build each pedal: its design, where its parts sit, and its print
    {
        const auto dir = folder.getChildFile ("gear");
        dir.createDirectory();
        juce::PNGImageFormat png;
        auto rect = [] (juce::Rectangle<float> r)
        {
            juce::Array<juce::var> a;
            for (float v : { r.getX(), r.getY(), r.getWidth(), r.getHeight() }) a.add (v);
            return juce::var (a);
        };
        auto hex = [] (juce::uint32 c) { return juce::String::toHexString ((juce::int64) (c & 0xffffff)).paddedLeft ('0', 6); };
        juce::Array<juce::var> pedals;
        struct Family { int module; int types; const char* name; };
        for (auto fam : { Family { mGate, 1, "gate" }, Family { mComp, 1, "comp" }, Family { mWah, 3, "wah" }, Family { mPitch, 1, "pitch" },
                          Family { mDrive, 7, "drive" }, Family { mEq, 1, "eq" }, Family { mMod, 6, "mod" }, Family { mDelay, 4, "delay" },
                          Family { mReverb, 6, "reverb" }, Family { mLofi, 1, "lofi" } })
            for (int t = 0; t < fam.types; ++t)
            {
                const auto& d = *pedalDesignFor (fam.module, t);
                const bool faders = fam.module == mEq;
                int n = faders ? 8 : (int) modules()[(size_t) fam.module].knobs.size();
                if (d.form == 5) --n; // the treadle is the first control
                const auto l = pedalLayout ({ 0.0f, 0.0f, 480.0f, 420.0f }, d, n, faders);
                const auto area = pedalArtArea (l.box);
                const auto id = juce::String (fam.name) + "-" + juce::String (t);
                auto* o = new juce::DynamicObject();
                o->setProperty ("id", id);
                o->setProperty ("name", d.name);
                o->setProperty ("body", hex (d.body));
                o->setProperty ("ink", hex (d.ink));
                o->setProperty ("led", hex (d.led));
                o->setProperty ("finish", (int) d.finish);
                o->setProperty ("knob", (int) d.knob);
                o->setProperty ("knobCap", hex (d.knobCap));
                o->setProperty ("stripe", d.stripe);
                o->setProperty ("form", l.form);
                o->setProperty ("variant", l.variant);
                o->setProperty ("faders", faders);
                o->setProperty ("area", rect (area));
                o->setProperty ("box", rect (l.box));
                o->setProperty ("knobs", rect (l.knobs));
                o->setProperty ("nameBox", rect (l.name));
                o->setProperty ("ledBox", rect (l.led));
                o->setProperty ("footswitch", rect (l.footswitch));
                o->setProperty ("treadle", rect (l.treadle));
                o->setProperty ("footStyle", footswitchStyle (l));
                juce::Array<juce::var> cells;
                if (! faders)
                    for (int i = 0; i < n; ++i)
                    {
                        const auto cell = pedalKnobCell (l, i, n);
                        const float size = (float) (int) std::min (cell.getWidth() - 6.0f, std::min (cell.getHeight() - 20.0f, 96.0f));
                        cells.add (rect (cell.withSizeKeepingCentre (size, size + 20.0f).removeFromTop (size)));
                    }
                o->setProperty ("knobCells", cells);
                pedals.add (juce::var (o));
                auto f = dir.getChildFile (id + "-print.png");
                f.deleteFile();
                if (auto out = f.createOutputStream()) png.writeImageToStream (renderPedalPrint (d, l, area, 2.0f), *out);
            }
        juce::Array<juce::var> amps;
        auto point = [] (juce::Point<float> p)
        {
            juce::Array<juce::var> a;
            a.add (p.x);
            a.add (p.y);
            return juce::var (a);
        };
        for (int m = 0; m < choices::numAmpModels; ++m)
        {
            const auto& d = ampDesign (m);
            const juce::Rectangle<float> place (0.0f, 0.0f, ampCanvasW, ampCanvasH);
            const auto l = ampLayout (place, d);
            const auto area = ampArtArea (place);
            const auto id = "amp-" + juce::String (m).paddedLeft ('0', 2);
            auto* o = new juce::DynamicObject();
            o->setProperty ("id", id);
            o->setProperty ("name", d.badge);
            o->setProperty ("tolex", (int) d.tolex);
            o->setProperty ("tolexColour", hex (d.tolexColour));
            o->setProperty ("panelKind", (int) d.panel);
            o->setProperty ("ink", hex (d.ink));
            o->setProperty ("weave", (int) d.weave);
            o->setProperty ("piping", d.piping != 0 ? hex (d.piping) : juce::String());
            o->setProperty ("knob", (int) d.knob);
            o->setProperty ("knobCap", hex (d.knobCap));
            o->setProperty ("jewelColour", hex (d.jewel));
            o->setProperty ("form", d.form);
            o->setProperty ("groups", d.groups);
            o->setProperty ("lamp", d.lamp);
            o->setProperty ("area", rect (area));
            o->setProperty ("head", rect (l.head));
            o->setProperty ("panel", rect (l.panel));
            o->setProperty ("grille", rect (l.grille));
            o->setProperty ("badge", rect (l.badge));
            o->setProperty ("jewel", rect (l.jewel));
            o->setProperty ("handle", rect (l.handle));
            o->setProperty ("rack", rect (l.rack));
            o->setProperty ("jack", point (l.jack));
            o->setProperty ("knobSize", l.knobSize);
            juce::Array<juce::var> cells;
            for (const auto& cell : l.cells) cells.add (rect (cell.withHeight (cell.getWidth())));
            o->setProperty ("knobCells", cells);
            juce::var raisedParts;
            const auto print = renderAmpPrint (d, l, area, 2.0f, raisedParts);
            o->setProperty ("relief", raisedParts);
            amps.add (juce::var (o));
            auto f = dir.getChildFile (id + "-print.png");
            f.deleteFile();
            if (auto out = f.createOutputStream()) png.writeImageToStream (print, *out);
        }
        auto* root = new juce::DynamicObject();
        root->setProperty ("scale", 2.0);
        root->setProperty ("pedals", pedals);
        root->setProperty ("amps", amps);
        dir.getChildFile ("gear.json").replaceWithText (juce::JSON::toString (juce::var (root)));
    }
    {
        // The whole window in the shapes people's screens give it: the rig has to fill each one,
        // with nothing cut off and nothing covered.
        const auto was = getBounds();
        auto shootWindow = [&] (const juce::String& name)
        {
            r.tick();
            const auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png;
            auto f = folder.getChildFile (name + ".png");
            f.deleteFile();
            if (auto out = f.createOutputStream()) png.writeImageToStream (img, *out);
        };
        struct Shape { const char* name; int w, h; };
        for (const auto& sh : { Shape { "16x9", 1600, 900 }, Shape { "16x10", 1440, 900 }, Shape { "21x9", 2100, 900 },
                                Shape { "4x3", 1024, 768 }, Shape { "tall", 900, 1100 }, Shape { "small", 700, 455 } })
        {
            setSize (sh.w, sh.h);
            r.closeOverlays();
            r.showPage (mAmp);
            shootWindow (juce::String ("window-") + sh.name);
            r.toggleOverlay (r.settings);
            shootWindow (juce::String ("window-") + sh.name + "-settings");
            r.closeOverlays();
        }
        // a take playing while Settings is open: the strip must not cover anything
        setSize (1600, 900);
        const auto takes = listTakes (proc.recordingFolder);
        if (! takes.empty())
        {
            r.takeBar.start (takes.front(), false);
            r.toast.setVisible (false);
            r.toggleOverlay (r.settings);
            r.settings.show (SettingsView::Section::app); // the page whose button the strip used to cover
            shootWindow ("window-settings-take");
            r.closeOverlays();
            r.takeBar.stop();
        }
        setBounds (was);
    }
}

bool RiffhouseEditor::keyPressed (const juce::KeyPress& k)
{
    if (root->wizard != nullptr) return false;
    // Inside a DAW the keyboard is the DAW's: space starts its transport, Ctrl+Z is its undo.
    if (proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone) return false;
    if (k == juce::KeyPress::escapeKey)
    {
        root->closeOverlays();
        return true;
    }
    if (k == juce::KeyPress (',', juce::ModifierKeys::commandModifier, 0))
    {
        root->closeOverlays();
        root->toggleOverlay (root->settings);
        return true;
    }
    if (k == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0))
    {
        root->showPopover (root->saveDialog, root->saveButton.getBounds(), 360, 206);
        return true;
    }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { root->stepHistory (false); return true; }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)) { root->stepHistory (true); return true; }
    if (k == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0))
    {
        // a tone a friend sent: the whole message from the chat, pasted anywhere in the app
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        if (! text.contains ("riffhouse:"))
        {
            // chords or tab copied from a website: they open on the song sheet
            if (! SongSheetView::looksLikeSong (text)) return false;
            if (root->openOverlay != &root->sheetView) root->toggleOverlay (root->sheetView);
            root->sheetView.pasteSong (text);
            return true;
        }
        Preset p;
        juce::String err;
        if (! presetFromShareCode (text, p, err))
        {
            showToast ("That tone code is cut short. Ask for it again and copy the whole message.");
            return true;
        }
        proc.applySharedPreset (p);
        root->closeOverlays();
        root->refreshTop();
        showToast ("Loaded \"" + p.name + "\" from your clipboard. Save it to keep it.");
        return true;
    }
    if (root->openOverlay == &root->browser || root->anyPopoverOpen()) return false;
    if (k == juce::KeyPress::leftKey) { root->stepPreset (-1); return true; }
    if (k == juce::KeyPress::rightKey) { root->stepPreset (1); return true; }
    if (k.getTextCharacter() == 'o' || k.getTextCharacter() == 'O')
    {
        // O is the looper's switch; with Shift it stops the loop
        const auto was = proc.looper().state();
        if (k.getModifiers().isShiftDown())
        {
            proc.looper().stop();
            if (was != Looper::State::empty) showToast ("Looper: stopped");
            return true;
        }
        proc.loopPress();
        showToast (was == Looper::State::empty ? "Looper: recording. Press O again to close the loop."
                   : was == Looper::State::playing ? "Looper: adding a layer"
                   : "Looper: playing");
        return true;
    }
    if (k.getTextCharacter() == 't' || k.getTextCharacter() == 'T') { root->toggleOverlay (root->tuner); return true; }
    if (k.getTextCharacter() == 'r' || k.getTextCharacter() == 'R') { root->toggleRecording(); return true; }
    if (k.getTextCharacter() == 'l' || k.getTextCharacter() == 'L') { root->toggleOverlay (root->takesView); return true; }
    if (k == juce::KeyPress::spaceKey)
    {
        // space is play and pause: the take if one is up, otherwise the song you play along with
        if (root->takeBar.isActive()) proc.takes().setPlaying (! proc.takes().isPlaying());
        else if (proc.jamTrackName().isNotEmpty()) proc.setJamPlaying (! proc.isJamPlaying());
        else return false;
        return true;
    }
    return false;
}
} // namespace rh
