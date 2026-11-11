// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Editor.h"
#include "../plugin/StandaloneHost.h"

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
            g.setFont (font (15.0f, Weight::semibold));
            g.drawText (formatTime (proc.recorder().secondsRecorded()), r, juce::Justification::centredLeft);
            return;
        }
        drawCaps (g, counting ? "Ready" : "Rec", r, 12.0f, hover || counting ? colours::text : colours::textDim, juce::Justification::centredLeft, 0.14f);
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
            if (idx >= 0)
            {
                const auto& pr = proc.presets().all()[(size_t) idx];
                sub = pr.inspiredBy.isNotEmpty() ? "Inspired by " + pr.inspiredBy : pr.category;
                if (! pr.genres.isEmpty()) sub << ", " << pr.genres.joinIntoString (", ").toLowerCase();
            }
            else
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
        const float nw = juce::GlyphArrangement::getStringWidth (nameFont, name);
        const float editedW = modified ? capsWidth ("Edited", 9.5f) + 10.0f : 0.0f;
        const float total = std::min (top.getWidth(), nw + 6.0f + 14.0f + editedW);
        auto line = top.withSizeKeepingCentre (total, top.getHeight());
        auto nameR = line.removeFromLeft (std::min (nw + 2.0f, line.getWidth() - 20.0f - editedW));
        g.setFont (nameFont);
        g.setColour (hover ? colours::accentHi : colours::text);
        g.drawText (name, nameR, juce::Justification::bottomLeft, true);
        line.removeFromLeft (4.0f);
        strokeIcon (g, icons::chevronDown(), line.removeFromLeft (14.0f).withTrimmedTop (line.getHeight() - 15.0f).withSizeKeepingCentre (11.0f, 11.0f),
                    hover ? colours::text : colours::textFaint, 1.6f);
        if (modified)
            drawCaps (g, "Edited", line.withTrimmedLeft (8.0f).withTrimmedTop (4.0f), 10.0f, colours::lamp, juce::Justification::bottomLeft);
        g.setFont (font (12.0f));
        g.setColour (colours::textFaint);
        g.drawText (sub, t.withTrimmedTop (2.0f), juce::Justification::centredTop, true);
    }

    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    RiffhouseProcessor& proc;
    juce::String name, sub;
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
          browser (p), tuner (p), settings (p), recordPanel (p), metroPanel (p), saveDialog (p)
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
        metroPanel.onChange = [this] { refreshTop(); };
        scrim.onClick = [this] { closePopovers(); };
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings,
                                    (juce::Component*) &scrim, (juce::Component*) &recordPanel, (juce::Component*) &metroPanel,
                                    (juce::Component*) &saveDialog })
            addChildComponent (c);
        addChildComponent (toast);
        addChildComponent (noticeBar);
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
            if (take.existsAsFile()) editor.showToast ("Saved " + take.getFileName());
        }
        else
        {
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
        toast.toFront (false);
    }

    void closeOverlays()
    {
        closePopovers();
        for (juce::Component* c : { (juce::Component*) &browser, (juce::Component*) &tuner, (juce::Component*) &settings })
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
        inMeter.setLevel (proc.meters().gateInPeak.load());
        outMeter.setLevel (proc.meters().outPeak.load());
        chain.refresh();
        if (! quietNotices && ++noticeTicks >= 10)
        {
            noticeTicks = 0;
            if (auto* host = StandaloneHost::get())
            {
                noticeBar.set (wizard == nullptr ? host->audioNotice() : StandaloneHost::Notice());
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

    juce::String bottomStatus() const
    {
        juce::StringArray s;
        const int mains = proc.meters().mains.load();
        const int humMode = juce::roundToInt (proc.value (P::humFilter));
        if (humMode == 0) s.add ("Hum filter off");
        else if (mains > 0) s.add (juce::String (mains) + " Hz hum removed");
        else s.add ("No hum");
        s.add (proc.value (P::inputFix) > 0.5f ? "Input fix on" : "Input fix off");
        s.add (juce::String ("Gate ") + (proc.value (P::gateOn) < 0.5f ? "off" : (proc.meters().gateOpen.load() ? "open" : "closed")));
        return s.joinIntoString ("   ");
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
        return juce::String (1000.0 * proc.getLatencySamples() / std::max (1.0, proc.currentSampleRate()), 2) + " ms added";
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
        recordOptions.setBounds (top.removeFromRight (20));
        record.setBounds (top.removeFromRight (72));
        top.removeFromRight (14);
        metroButton.setBounds (top.removeFromRight (70));
        tunerButton.setBounds (top.removeFromRight (36));

        // the tone's name sits dead centre between its two arrows; star and save follow on the right
        auto centre = juce::Rectangle<int> (0, 0, 352, topH).withCentre ({ getWidth() / 2, topH / 2 }).reduced (0, 8);
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
        inMeter.setBounds (inMeterArea.getX() + 62, inMeterArea.getY() + 21, 224, 8);
        outMeterArea = bottom.removeFromRight (380);
        outMeter.setBounds (outMeterArea.getRight() - 224, outMeterArea.getY() + 21, 224, 8);
        auto macros = bottom.withSizeKeepingCentre (3 * 92, getHeight() - bottomY);
        for (auto* k : { &space, &doubler, &volume })
            k->setBounds (macros.removeFromLeft (92).withSizeKeepingCentre (64, 58));

        const auto overlay = juce::Rectangle<int> (0, topH + 1, getWidth(), getHeight() - topH - 1);
        browser.setBounds (overlay);
        tuner.setBounds (overlay);
        settings.setBounds (overlay);
        toast.setBounds (getWidth() / 2 - 300, bottomY - 60, 600, 44);
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

        // bottom bar
        drawCaps (g, "Input", { (float) inMeterArea.getX(), (float) inMeterArea.getY() + 16.0f, 56.0f, 18.0f }, 11.5f, colours::textDim, juce::Justification::centredLeft, 0.14f);
        drawCaps (g, "Output", { (float) outMeterArea.getRight() - 300.0f, (float) outMeterArea.getY() + 16.0f, 70.0f, 18.0f }, 11.5f, colours::textDim, juce::Justification::centredLeft, 0.14f);
        drawCaps (g, lastStatus, { (float) inMeterArea.getX(), (float) inMeterArea.getY() + 38.0f, 380.0f, 16.0f }, 11.0f, colours::textDim, juce::Justification::centredLeft, 0.1f);
        const int clips = proc.meters().clipEvents.load();
        const auto rightLine = juce::Rectangle<float> ((float) outMeterArea.getX(), (float) outMeterArea.getY() + 38.0f, 380.0f, 16.0f);
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
    juce::Rectangle<int> logoArea, inMeterArea, outMeterArea;
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
    NoticeBar noticeBar;
    int noticeTicks = 0;
    bool quietNotices = false; // pictures of the app are taken without a message about the missing guitar
    std::unique_ptr<SetupWizard> wizard;

    juce::String lastStatus;
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
    return false;
}
} // namespace rh
