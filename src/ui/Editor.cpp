// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Editor.h"
#include "../plugin/StandaloneHost.h"
#include "TakesView.h"

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
        // the mark, then the name in condensed capitals, like the badge on a head
        drawBrandMark (g, r.removeFromLeft (30.0f).withSizeKeepingCentre (30.0f, 30.0f).translated (0.0f, -1.5f), colours::text);
        r.removeFromLeft (9.0f);
        g.setColour (colours::text);
        g.setFont (faceFont (Face::shoulders, 21.0f).withExtraKerningFactor (0.16f));
        g.drawText ("RIFFHOUSE", r, juce::Justification::centredLeft);
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
                if (sub.isEmpty()) sub = pr.inspiredBy.isNotEmpty() ? "Inspired by " + pr.inspiredBy : pr.category;
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
        alpha = 1.0f;
        hold = 75;
        setVisible (true);
        startTimerHz (30);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        g.setFont (font (14.5f, Weight::medium));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 40.0f;
        const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (std::min (w, (float) getWidth()), 40.0f);
        g.setColour (juce::Colour (0xff232221).withAlpha (alpha));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f * alpha));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (colours::text.withAlpha (alpha));
        g.drawText (text, r, juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        if (--hold > 0) return;
        alpha -= 0.08f;
        if (alpha <= 0.0f) { setVisible (false); stopTimer(); }
        repaint();
    }
    juce::String text;
    float alpha = 0.0f;
    int hold = 0;
};

// A message about the audio connection that stays until the problem is gone: the input was
// unplugged, audio is restarting, Bluetooth headphones add delay. One optional action.
class NoticeBar : public juce::Component
{
public:
    void set (const StandaloneHost::Notice& n)
    {
        if (n.text == notice.text && n.action == notice.action) return;
        notice = n;
        setVisible (n.text.isNotEmpty());
        repaint();
    }
    float wantedWidth() const
    {
        float w = 52.0f + juce::GlyphArrangement::getStringWidth (font (13.5f, Weight::medium), notice.text);
        if (notice.action.isNotEmpty()) w += 34.0f + juce::GlyphArrangement::getStringWidth (font (13.5f, Weight::semibold), notice.action);
        return w;
    }
    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff232221));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour ((notice.warning ? colours::hot : juce::Colours::white).withAlpha (notice.warning ? 0.45f : 0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto row = r.reduced (16.0f, 0.0f);
        g.setColour (notice.warning ? colours::hot : colours::textDim);
        g.fillEllipse (row.removeFromLeft (8.0f).withSizeKeepingCentre (8.0f, 8.0f));
        row.removeFromLeft (12.0f);
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
        if (o != over) { over = o; setMouseCursor (o ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && actionArea.expanded (8.0f).contains (e.position) && onAction) onAction();
    }
    std::function<void()> onAction;

private:
    StandaloneHost::Notice notice;
    juce::Rectangle<float> actionArea;
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
          browser (p), tuner (p), settings (p), recordPanel (p), metroPanel (p), saveDialog (p),
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
        saveButton.onClick = [this] { showPopover (saveDialog, saveButton.getBounds(), 360, 230); };
        tunerButton.setTooltip ("Tuner (T)");
        tunerButton.onClick = [this] { toggleOverlay (tuner); };
        metroButton.setTooltip ("Metronome");
        metroButton.onClick = [this] { showPopover (metroPanel, metroButton.getBounds(), 300, 300); };
        settingsButton.setTooltip ("Settings");
        settingsButton.onClick = [this] { toggleOverlay (settings); };
        recordOptions.setTooltip ("Recording options and play-along");
        recordOptions.onClick = [this] { showPopover (recordPanel, recordOptions.getBounds(), 380, 470); };
        record.onClick = [this] { toggleRecording(); };
        for (auto* b : { &favourite, &saveButton, &tunerButton, &metroButton, &settingsButton, &recordOptions })
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

        // overlays
        browser.onClose = [this] { closeOverlays(); };
        browser.onLoaded = [this] { refreshTop(); };
        browser.onMessage = [this] (juce::String m) { editor.showToast (m); };
        tuner.onClose = [this] { closeOverlays(); };
        settings.onClose = [this] { closeOverlays(); };
        settings.onRunSetup = [this] { closeOverlays(); editor.showSetupWizard(); };
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
            placeBars();
            if (takesView.isVisible()) takesView.refresh();
        };
        recordPanel.onShowTakes = [this] { closePopovers(); toggleOverlay (takesView); };
        metroPanel.onChange = [this] { refreshTop(); };
        scrim.onClick = [this] { closePopovers(); };
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings,
                                    (juce::Component*) &takesView, (juce::Component*) &scrim, (juce::Component*) &recordPanel, (juce::Component*) &metroPanel,
                                    (juce::Component*) &saveDialog })
            addChildComponent (c);
        addChildComponent (dropHint);
        addChildComponent (toast);
        addChildComponent (noticeBar);
        addChildComponent (takeBar);
        noticeBar.onAction = [] { if (auto* host = StandaloneHost::get()) host->audioNoticeAction(); };

        showPage ((int) proc.uiState.getProperty ("page", (int) mAmp));
        refreshTop();
    }

    // -------------------------------------------------------------------------
    void showPage (int i)
    {
        i = juce::jlimit (0, numModules - 1, i);
        chain.setSelected (i);
        amp.setVisible (i == mAmp);
        for (int k = 0; k < pages.size(); ++k)
            if (pages[k] != nullptr) pages[k]->setVisible (k == i);
        proc.uiState.setProperty ("page", i, nullptr);
    }

    void stepPreset (int d)
    {
        const int n = (int) proc.presets().all().size();
        if (n == 0) return;
        const int cur = proc.currentPresetIndex();
        proc.loadPreset (cur < 0 ? 0 : (cur + d + n) % n);
        refreshTop();
        editor.showToast (proc.currentPresetName());
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
        takeBar.toFront (false);
        toast.toFront (false);
    }

    // The strip for a playing take and the audio notice share one place above the bottom bar.
    void placeBars()
    {
        if (takeBar.isActive())
        {
            const int w = juce::jmin (getWidth() - 48, 900);
            takeBar.setBounds ((getWidth() - w) / 2, bottomY - 40, w, 32);
            takeBar.toFront (false);
            noticeBar.setVisible (false);
        }
    }

    void closeOverlays()
    {
        closePopovers();
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings, (juce::Component*) &takesView })
            c->setVisible (false);
        openOverlay = nullptr;
        proc.tunerOpen.store (false);
        tunerButton.setToggleState (false, juce::dontSendNotification);
        settingsButton.setToggleState (false, juce::dontSendNotification);
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
        for (Popover* p : { (Popover*) &recordPanel, (Popover*) &metroPanel, (Popover*) &saveDialog })
            p->setVisible (false);
    }

    bool anyPopoverOpen() const { return scrim.isVisible(); }

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
        if (! quietNotices && ++noticeTicks >= 10)
        {
            noticeTicks = 0;
            if (auto* host = StandaloneHost::get())
            {
                noticeBar.set (wizard == nullptr && ! takeBar.isActive() ? host->audioNotice() : StandaloneHost::Notice());
                if (noticeBar.isVisible())
                {
                    const int w = juce::jmin (getWidth() - 48, juce::roundToInt (noticeBar.wantedWidth()));
                    noticeBar.setBounds ((getWidth() - w) / 2, bottomY - 40, w, 32);
                }
            }
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
        if (status != lastStatus) { lastStatus = status; repaint (0, bottomY, getWidth(), getHeight() - bottomY); }
        if (++frame % 15 == 0) refreshTop();
    }

    struct StatusItem { juce::String text; bool lit; };
    std::vector<StatusItem> statusItems() const
    {
        std::vector<StatusItem> s;
        const int mains = proc.meters().mains.load();
        const int humMode = juce::roundToInt (proc.value (P::humFilter));
        if (humMode == 0) s.push_back ({ "Hum filter off", false });
        else if (mains > 0) s.push_back ({ juce::String (mains) + " Hz hum removed", true });
        else s.push_back ({ "No hum found", false });
        const bool fix = proc.value (P::inputFix) > 0.5f;
        s.push_back ({ fix ? "Input fix on" : "Input fix off", fix });
        const bool gate = proc.value (P::gateOn) > 0.5f;
        const bool open = gate && proc.meters().gateOpen.load();
        s.push_back ({ juce::String ("Gate ") + (! gate ? "off" : (open ? "open" : "closed")), open });
        return s;
    }

    juce::String bottomStatus() const
    {
        juce::String all;
        for (const auto& item : statusItems()) all << item.text << (item.lit ? "*" : "-");
        all << inMeter.peakText() << outMeter.peakText();
        return all;
    }

    juce::String latencyText() const
    {
        if (auto* host = StandaloneHost::get())
            if (auto* dev = host->deviceManager().getCurrentAudioDevice())
            {
                int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
                if (lat <= 0) lat = 2 * dev->getCurrentBufferSizeSamples();
                lat += proc.getLatencySamples();
                return juce::String (1000.0 * lat / std::max (1.0, dev->getCurrentSampleRate()), 1) + " ms round trip";
            }
        return "The rig adds " + juce::String (1000.0 * proc.getLatencySamples() / std::max (1.0, proc.currentSampleRate()), 2) + " ms";
    }

    // -------------------------------------------------------------------------
    static constexpr int topH = 56, chainY = 66, chainH = 98, pageY = 176, bottomY = 712;

    void resized() override
    {
        // top bar: name on the left, the tone in the middle, tools on the right
        auto top = juce::Rectangle<int> (0, 0, getWidth(), topH).reduced (24, 10);
        logoArea = top.removeFromLeft (180);
        settingsButton.setBounds (top.removeFromRight (36));
        top.removeFromRight (10);
        recordOptions.setBounds (top.removeFromRight (18));
        record.setBounds (top.removeFromRight (84));
        top.removeFromRight (14);
        metroButton.setBounds (top.removeFromRight (70));
        tunerButton.setBounds (top.removeFromRight (36));

        // the tone's name sits dead centre between its two arrows; star and save follow on the right
        auto centre = juce::Rectangle<int> (0, 0, 380, topH).withCentre ({ getWidth() / 2, topH / 2 }).reduced (0, 8);
        toneWell = centre;
        prevPreset.setBounds (centre.removeFromLeft (28));
        nextPreset.setBounds (centre.removeFromRight (28));
        auto right = juce::Rectangle<int> (centre.getRight() + 28, centre.getY(), 6 + 30 + 2 + 30, centre.getHeight());
        right.removeFromLeft (6);
        favourite.setBounds (right.removeFromLeft (30));
        right.removeFromLeft (2);
        saveButton.setBounds (right.removeFromLeft (30));
        presetBox.setBounds (centre.reduced (4, 0));

        chain.setBounds (0, chainY, getWidth(), chainH);
        const auto page = juce::Rectangle<int> (24, pageY, getWidth() - 48, bottomY - pageY - 6);
        amp.setBounds (page);
        for (auto* pg : pages) if (pg != nullptr) pg->setBounds (page);

        // bottom bar: input on the left, the three big-picture knobs in the middle, output on the right
        auto bottom = juce::Rectangle<int> (24, bottomY, getWidth() - 48, getHeight() - bottomY);
        inMeterArea = bottom.removeFromLeft (380);
        inMeter.setBounds (inMeterArea.getX() + 62, inMeterArea.getY() + 17, 226, 13);
        outMeterArea = bottom.removeFromRight (380);
        outMeter.setBounds (outMeterArea.getRight() - 226 - 54, outMeterArea.getY() + 17, 226, 13);
        auto macros = bottom.withSizeKeepingCentre (3 * 92, getHeight() - bottomY);
        for (auto* k : { &space, &doubler, &volume })
            k->setBounds (macros.removeFromLeft (92).withSizeKeepingCentre (64, 58));

        const auto overlay = juce::Rectangle<int> (0, topH + 1, getWidth(), getHeight() - topH - 1);
        browser.setBounds (overlay);
        tuner.setBounds (overlay);
        settings.setBounds (overlay);
        takesView.setBounds (overlay);
        placeBars();
        toast.setBounds (getWidth() / 2 - 300, bottomY - 60, 600, 44);
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
            sg.fillRect (0, bottomY, getWidth(), getHeight() - bottomY);
            sg.setColour (juce::Colour (0xff222120));
            sg.fillRect (0, topH, getWidth(), 1);
            sg.fillRect (0, bottomY, getWidth(), 1);
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
    IconButton tunerButton { icons::tuningFork() }, metroButton { icons::metronome(), "100" }, settingsButton { icons::gear() };
    RecordButton record;
    IconButton recordOptions { icons::chevronDown() };

    ChainStrip chain;
    AmpPage amp;
    juce::OwnedArray<ModulePage> pages;

    Knob space, doubler, volume;
    Meter inMeter, outMeter;

    PresetBrowser browser;
    TunerView tuner;
    SettingsView settings;
    Scrim scrim;
    RecordPanel recordPanel;
    MetronomePanel metroPanel;
    SaveDialog saveDialog;
    Toast toast;
    DropHint dropHint;
    NoticeBar noticeBar;
    TakeBar takeBar;
    TakesView takesView;
    int noticeTicks = 0;
    bool quietNotices = false; // pictures of the app are taken without a message about the missing guitar
    std::unique_ptr<SetupWizard> wizard;

    juce::String lastStatus;
    bool demoLevels = false;
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
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    const double scale = (double) proc.uiState.getProperty ("scale", 1.0);
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
    setWantsKeyboardFocus (true);

    if (auto* host = StandaloneHost::get())
    {
        auto* dev = host->deviceManager().getCurrentAudioDevice();
        const bool noInput = dev == nullptr || dev->getActiveInputChannels().isZero();
        if (! host->settings().getBoolValue ("setupDone", false) || noInput)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RiffhouseEditor> (this)]
                                             { if (safe != nullptr) safe->showSetupWizard(); });
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

void RiffhouseEditor::paint (juce::Graphics& g) { g.fillAll (colours::bg); }

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
    const float s = (float) getWidth() / (float) baseWidth;
    root->setTransform (juce::AffineTransform::scale (s));
    root->setBounds (0, 0, baseWidth, baseHeight);
    proc.uiState.setProperty ("scale", s, nullptr);
}

void RiffhouseEditor::timerCallback()
{
    root->tick();
}

void RiffhouseEditor::showSetupWizard()
{
    if (StandaloneHost::get() == nullptr || root->wizard != nullptr) return;
    root->closeOverlays();
    root->wizard = std::make_unique<SetupWizard> (proc);
    root->addAndMakeVisible (*root->wizard);
    root->wizard->setBounds (root->getLocalBounds());
    root->wizard->onFinished = [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<RiffhouseEditor> (this)]
        {
            if (safe == nullptr) return;
            safe->root->wizard.reset();
            safe->root->repaint();
        });
    };
    root->wizard->begin();
}

namespace
{
enum class Dropped { none, capture, tone, sound };

Dropped kindOf (const juce::File& f)
{
    const auto ext = f.getFileExtension().toLowerCase();
    if (ext == ".nam") return Dropped::capture;
    if (ext == ".json") return Dropped::tone;
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
            line = proc.audioFileSeconds (first) <= kLongestImpulseSeconds
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
        case Dropped::sound:
        {
            const double seconds = proc.audioFileSeconds (f);
            if (seconds <= 0.0)
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
    root->toast.setTopLeftPosition (root->toast.getX(), Root::bottomY - (root->noticeBar.isVisible() ? 92 : 60));
    root->toast.toFront (false);
    root->toast.show (m);
}

void RiffhouseEditor::saveSnapshots (const juce::File& folder, float scale)
{
    folder.createDirectory();
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
        const int w = juce::roundToInt (r.noticeBar.wantedWidth());
        r.noticeBar.setBounds ((r.getWidth() - w) / 2, Root::bottomY - 40, w, 32);
        shoot ("notice");
        r.noticeBar.set ({});
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
    shoot ("settings");
    r.closeOverlays();
    r.showPopover (r.recordPanel, r.recordOptions.getBounds(), 380, 470);
    shoot ("record");
    r.closePopovers();
    r.showPopover (r.metroPanel, r.metroButton.getBounds(), 300, 300);
    shoot ("metronome");
    r.closePopovers();
    r.showPopover (r.saveDialog, r.saveButton.getBounds(), 360, 230);
    shoot ("save");
    r.closePopovers();
    {
        SetupWizard w (proc);
        r.addAndMakeVisible (w);
        w.setBounds (r.getLocalBounds());
        w.begin();
        shoot ("setup");
        r.removeChildComponent (&w);
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
        const float big = 3.0f;
        struct Family { int module; int types; const char* name; };
        for (auto fam : { Family { mGate, 1, "gate" }, Family { mComp, 1, "comp" }, Family { mWah, 1, "wah" }, Family { mPitch, 1, "pitch" },
                          Family { mDrive, 7, "drive" }, Family { mEq, 1, "eq" }, Family { mMod, 6, "mod" }, Family { mDelay, 4, "delay" },
                          Family { mReverb, 6, "reverb" }, Family { mLofi, 1, "lofi" } })
            for (int t = 0; t < fam.types; ++t)
            {
                const auto& d = *pedalDesignFor (fam.module, t);
                const bool faders = fam.module == mEq;
                auto thumb = pedalThumb (d, faders ? 8 : (int) modules()[(size_t) fam.module].knobs.size(), faders, big);
                juce::Graphics tg (thumb.image);
                tg.addTransform (juce::AffineTransform::translation (-thumb.area.getX(), -thumb.area.getY()).scaled (big));
                drawPedalLed (tg, thumb.pedal, d, true);
                write (juce::String (fam.name) + "-" + juce::String (t), thumb.image);
            }
        for (int m = 0; m < choices::numAmpModels; ++m)
        {
            const auto& d = ampDesign (m);
            auto thumb = ampThumb (d, m == choices::neuralCapture ? juce::String ("your capture") : juce::String (spec (P::ampModel).choices[(size_t) m]).toLowerCase(), 1.6f);
            juce::Graphics tg (thumb.image);
            tg.addTransform (juce::AffineTransform::scale (1.6f));
            drawAmpJewel (tg, thumb.amp, d, true);
            write ("amp-" + juce::String (m).paddedLeft ('0', 2), thumb.image);
        }
        for (int m = 0; m < 8; ++m)
            write ("cab-" + juce::String (m), cabThumb (m, 2.0f).image);
    }
}

bool RiffhouseEditor::keyPressed (const juce::KeyPress& k)
{
    if (root->wizard != nullptr) return false;
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
        root->showPopover (root->saveDialog, root->saveButton.getBounds(), 360, 230);
        return true;
    }
    if (root->openOverlay == &root->browser || root->anyPopoverOpen()) return false;
    if (k == juce::KeyPress::leftKey) { root->stepPreset (-1); return true; }
    if (k == juce::KeyPress::rightKey) { root->stepPreset (1); return true; }
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
