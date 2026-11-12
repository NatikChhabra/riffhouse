// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "TakesView.h"
#include "Theme.h"

namespace rh::ui
{
juce::String TakeBar::clock (double seconds)
{
    const int s = juce::jmax (0, juce::roundToInt (std::floor (seconds)));
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}

TakeBar::TakeBar (RiffhouseProcessor& p) : proc (p)
{
    play.drawBackground = false;
    play.iconSize = 15.0f;
    play.iconColour = colours::text;
    play.onClick = [this] { proc.takes().setPlaying (! proc.takes().isPlaying()); timerCallback(); };
    play.setTooltip ("Play or pause this take (space)");
    close.drawBackground = false;
    close.iconSize = 13.0f;
    close.onClick = [this] { stop(); };
    close.setTooltip ("Stop and go back to your live guitar");
    pos.setSliderStyle (juce::Slider::LinearHorizontal);
    pos.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pos.setRange (0.0, 1.0);
    pos.onDragEnd = [this] { proc.takes().setPosition (pos.getValue() * proc.takes().length()); };
    action.onClick = [this]
    {
        if (job != nullptr) return;
        if (isChangingTone()) save();
        else start (proc.takes().take(), true);
    };
    for (juce::Component* c : { (juce::Component*) &play, (juce::Component*) &close, (juce::Component*) &pos, (juce::Component*) &action })
        addAndMakeVisible (c);
    setVisible (false);
}

TakeBar::~TakeBar()
{
    job.reset();
    proc.takes().unload();
}

void TakeBar::start (const Take& t, bool changeTone)
{
    if (job != nullptr) return;
    auto& player = proc.takes();
    // switching between the two ways of hearing the same take keeps your place
    const double keep = player.isLoaded() && player.take().sound == t.sound ? player.position() : 0.0;
    const bool tone = changeTone && t.hasClean();
    if (! player.load (t, tone ? TakePlayer::Mode::throughRig : TakePlayer::Mode::asRecorded))
    {
        if (onMessage) onMessage ("That take could not be opened. It may have been moved or deleted.");
        return;
    }
    if (keep > 0.0 && keep < player.length() - 0.5) player.setPosition (keep);
    player.setPlaying (true);
    setVisible (true);
    startTimerHz (15);
    timerCallback();
    resized();
    repaint();
    if (onChanged) onChanged();
}

void TakeBar::stop()
{
    if (job != nullptr) return; // a save is running: it finishes first
    stopTimer();
    proc.takes().unload();
    setVisible (false);
    if (onChanged) onChanged();
}

void TakeBar::save()
{
    auto& player = proc.takes();
    if (! player.isLoaded() || job != nullptr) return;
    const auto take = player.take();
    player.setPlaying (false);
    action.setEnabled (false);
    job = std::make_unique<ReampJob> (take, proc.reampSetup(), [this] (juce::File result, juce::String error)
    {
        job.reset();
        action.setEnabled (true);
        if (error.isNotEmpty())
        {
            if (onMessage) onMessage (error);
        }
        else
        {
            if (onMessage) onMessage ("Saved as a new take: " + result.getFileNameWithoutExtension());
            // hand over to the new take as it was saved, ready to hear
            for (const auto& t : listTakes (result.getParentDirectory()))
                if (t.sound == result)
                {
                    proc.takes().load (t, TakePlayer::Mode::asRecorded);
                    break;
                }
        }
        timerCallback();
        resized();
        repaint();
        if (onChanged) onChanged();
    });
    repaint();
}

void TakeBar::timerCallback()
{
    auto& player = proc.takes();
    play.setIcon (player.isPlaying() ? icons::pause() : icons::play(), true);
    const double len = player.length();
    if (len > 0.0 && ! pos.isMouseButtonDown())
        pos.setValue (player.position() / len, juce::dontSendNotification);
    const juce::String label = job != nullptr ? "Saving " + juce::String (juce::roundToInt (job->progress() * 100.0f)) + "%"
                                              : (isChangingTone() ? juce::String ("Save as new take") : juce::String ("Change the tone"));
    if (action.getButtonText() != label) action.setButtonText (label);
    action.setVisible (job != nullptr || isChangingTone() || player.take().hasClean());
    repaint (timeArea);
}

void TakeBar::resized()
{
    auto r = getLocalBounds().reduced (10, 0);
    play.setBounds (r.removeFromLeft (30).withSizeKeepingCentre (30, 30));
    r.removeFromLeft (6);
    close.setBounds (r.removeFromRight (28).withSizeKeepingCentre (28, 28));
    r.removeFromRight (8);
    action.setBounds (r.removeFromRight (150).withSizeKeepingCentre (150, 26));
    r.removeFromRight (14);
    nameArea = r.removeFromLeft (juce::jmin (300, r.getWidth() / 2));
    r.removeFromLeft (12);
    timeArea = r.removeFromRight (86);
    r.removeFromRight (8);
    pos.setBounds (r);
}

void TakeBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff232221));
    g.fillRoundedRectangle (r, 6.0f);
    const bool tone = isChangingTone();
    g.setColour (tone ? colours::lamp.withAlpha (0.55f) : juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto name = nameArea;
    const juce::String lead = tone ? "NEW TONE ON" : "TAKE";
    const float lw = capsWidth (lead, 10.5f, 0.12f) + 10.0f;
    drawCaps (g, lead, name.removeFromLeft ((int) lw).toFloat(), 10.5f, tone ? colours::lamp : colours::textDim, juce::Justification::centredLeft, 0.12f);
    g.setColour (colours::text);
    g.setFont (font (13.5f, Weight::medium));
    g.drawText (proc.takes().take().name, name, juce::Justification::centredLeft, true);

    g.setColour (colours::textDim);
    g.setFont (font (12.5f));
    g.drawText (clock (proc.takes().position()) + " / " + clock (proc.takes().length()), timeArea, juce::Justification::centredRight);
}

//==============================================================================
TakesView::TakesView (RiffhouseProcessor& p, TakeBar& b) : proc (p), bar (b)
{
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);
    openFolder.onClick = [this]
    {
        proc.recordingFolder.createDirectory();
        proc.recordingFolder.startAsProcess();
    };
    addAndMakeVisible (openFolder);
    convert.setTooltip ("Turn any WAV, AIFF, FLAC, OGG or MP3 into an MP3 or FLAC, saved beside it. Dropping the file on this list does the same.");
    convert.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Which sound file?", proc.recordingFolder, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) offerCopy (f, f.getFileNameWithoutExtension(), f.getParentDirectory(), &convert);
                              });
    };
    addAndMakeVisible (convert);
    list.setModel (this);
    list.setRowHeight (54);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    addAndMakeVisible (list);
}

void TakesView::visibilityChanged()
{
    if (isVisible()) refresh();
}

void TakesView::refresh()
{
    takes = listTakes (proc.recordingFolder);
    list.updateContent();
    list.repaint();
    repaint();
}

void TakesView::resized()
{
    auto r = getLocalBounds().reduced (60, 40);
    auto head = r.removeFromTop (50);
    close.setBounds (head.removeFromRight (40).withSizeKeepingCentre (36, 36));
    head.removeFromRight (16);
    openFolder.setBounds (head.removeFromRight (150).withSizeKeepingCentre (150, 32));
    head.removeFromRight (10);
    convert.setBounds (head.removeFromRight (180).withSizeKeepingCentre (180, 32));
    r.removeFromTop (34);
    r.removeFromBottom (76); // the playing strip sits here
    list.setBounds (r);
}

void TakesView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    auto r = getLocalBounds().reduced (60, 40);
    g.setColour (colours::text);
    g.setFont (font (28.0f, Weight::display));
    g.drawText ("Takes", r.removeFromTop (50), juce::Justification::centredLeft);
    g.setFont (font (13.5f));
    g.setColour (colours::textDim);
    g.drawText ("Click a take to hear it. Change the tone plays its clean guitar through the rig, so you can pick another sound after recording.",
                r.removeFromTop (24), juce::Justification::centredLeft, true);
    if (takes.empty())
    {
        g.setFont (font (16.0f));
        g.drawFittedText ("No takes yet. Press R, or the REC button, and play something.",
                          list.getBounds().withHeight (140), juce::Justification::centred, 2, 1.0f);
    }
}

TakesView::Cells TakesView::cells (int w, int h, bool hasClean) const
{
    Cells c;
    auto r = juce::Rectangle<int> (w, h).reduced (18, 0);
    c.play = r.removeFromLeft (34);
    r.removeFromLeft (10);
    c.remove = r.removeFromRight ((int) capsWidth ("Delete", 10.5f, 0.12f) + 6);
    r.removeFromRight (26);
    c.reveal = r.removeFromRight ((int) capsWidth ("Show file", 10.5f, 0.12f) + 6);
    r.removeFromRight (26);
    c.share = r.removeFromRight ((int) capsWidth ("MP3 / FLAC", 10.5f, 0.12f) + 6);
    r.removeFromRight (26);
    c.tone = r.removeFromRight ((int) capsWidth ("Change the tone", 10.5f, 0.12f) + 6);
    if (! hasClean) c.tone = {};
    r.removeFromRight (34);
    c.length = r.removeFromRight (54);
    r.removeFromRight (20);
    c.when = r.removeFromRight (130);
    r.removeFromRight (16);
    c.name = r;
    return c;
}

void TakesView::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool)
{
    if (! juce::isPositiveAndBelow (row, (int) takes.size())) return;
    const auto& t = takes[(size_t) row];
    const bool current = bar.isActive() && bar.current() == t.sound;
    if (current)
    {
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillRoundedRectangle (juce::Rectangle<int> (w, h).reduced (2, 3).toFloat(), 8.0f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        g.fillRect (18.0f, (float) h - 1.0f, (float) w - 36.0f, 1.0f);
    }
    const auto c = cells (w, h, t.hasClean());
    fillIcon (g, current && proc.takes().isPlaying() ? icons::pause() : icons::play(), c.play.toFloat().withSizeKeepingCentre (15.0f, 15.0f),
              current ? colours::accent : colours::textDim);
    g.setFont (font (16.0f, Weight::semibold));
    g.setColour (colours::text);
    g.drawText (t.name, c.name, juce::Justification::centredLeft, true);
    g.setFont (font (13.0f));
    g.setColour (colours::textDim);
    const bool today = t.when.getDayOfYear() == juce::Time::getCurrentTime().getDayOfYear() && t.when.getYear() == juce::Time::getCurrentTime().getYear();
    g.drawText (today ? "Today, " + t.when.formatted ("%H:%M") : t.when.formatted ("%d %b, %H:%M"), c.when, juce::Justification::centredRight);
    const int s = juce::roundToInt (t.seconds);
    g.drawText (juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2), c.length, juce::Justification::centredRight);
    if (! c.tone.isEmpty())
        drawCaps (g, "Change the tone", c.tone.toFloat(), 10.5f, colours::accent, juce::Justification::centredRight, 0.12f);
    drawCaps (g, "MP3 / FLAC", c.share.toFloat(), 10.5f, colours::textDim, juce::Justification::centredRight, 0.12f);
    drawCaps (g, "Show file", c.reveal.toFloat(), 10.5f, colours::textDim, juce::Justification::centredRight, 0.12f);
    drawCaps (g, "Delete", c.remove.toFloat(), 10.5f, colours::textDim, juce::Justification::centredRight, 0.12f);
}

void TakesView::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (! juce::isPositiveAndBelow (row, (int) takes.size())) return;
    const auto t = takes[(size_t) row];
    const auto c = cells (list.getVisibleRowWidth(), list.getRowHeight(), t.hasClean());
    const auto at = e.getPosition();
    if (c.remove.expanded (8, 0).contains (at))
    {
        if (bar.isActive() && bar.current() == t.sound) bar.stop();
        // to the recycle bin, never gone for good; the clean guitar stays while another take still uses it
        const bool ok = t.sound.moveToTrash();
        bool shared = false;
        for (const auto& other : takes) shared = shared || (other.sound != t.sound && other.clean == t.clean);
        if (ok && ! shared && t.hasClean()) t.clean.moveToTrash();
        if (onMessage) onMessage (ok ? "Moved to the recycle bin: " + t.name : juce::String ("That take could not be deleted. Is the file open somewhere else?"));
        refresh();
        return;
    }
    if (c.reveal.expanded (8, 0).contains (at)) { t.sound.revealToUser(); return; }
    if (c.share.expanded (8, 0).contains (at))
    {
        offerCopy (t.sound, t.name, exportFileFor (proc.recordingFolder, t.sound, ExportFormat::mp3).getParentDirectory(), nullptr);
        return;
    }
    const bool tone = ! c.tone.isEmpty() && c.tone.expanded (8, 0).contains (at);
    if (! tone && bar.isActive() && bar.current() == t.sound && ! bar.isChangingTone())
    {
        proc.takes().setPlaying (! proc.takes().isPlaying()); // a second click on the playing take pauses it
        list.repaint();
        return;
    }
    if (onPlay) onPlay (t, tone);
    list.repaint();
}

void TakesView::convertDropped (const juce::File& f)
{
    if (f.existsAsFile()) offerCopy (f, f.getFileNameWithoutExtension(), f.getParentDirectory(), &convert);
}

void TakesView::offerCopy (const juce::File& source, const juce::String& title, juce::File folder, juce::Component* near)
{
    if (copying && copyJob != nullptr)
    {
        if (onMessage) onMessage ("Still saving " + copyJob->output().getFileName() + ". One moment.");
        return;
    }
    juce::PopupMenu m;
    m.addSectionHeader ("Save a copy of " + source.getFileName());
    m.addItem (1, "MP3: to send or post (small, plays everywhere)");
    m.addItem (2, "FLAC: to keep (every detail, half the size of WAV)");
    auto options = juce::PopupMenu::Options();
    options = near != nullptr ? options.withTargetComponent (near) : options.withMousePosition();
    m.showMenuAsync (options, [safe = juce::Component::SafePointer<TakesView> (this), source, title, folder] (int chosen)
    {
        if (safe == nullptr || chosen == 0) return;
        const auto format = chosen == 1 ? ExportFormat::mp3 : ExportFormat::flac;
        auto out = folder.getChildFile (source.getFileNameWithoutExtension() + exportExtension (format));
        // a copy is never written over the file it was made from, nor over something else of the player's
        if (out == source || (out.existsAsFile() && folder == source.getParentDirectory())) out = out.getNonexistentSibling();
        safe->startCopy (source, title, out, format);
    });
}

void TakesView::startCopy (const juce::File& source, const juce::String& title, const juce::File& out, ExportFormat format)
{
    if (onMessage) onMessage ("Saving " + out.getFileName() + "...");
    copying = true;
    copyJob = std::make_unique<ExportJob> (source, out, format, title,
                                           [safe = juce::Component::SafePointer<TakesView> (this), format] (juce::File made, juce::String err)
                                           {
                                               if (safe == nullptr) return;
                                               safe->copying = false;
                                               if (made.existsAsFile())
                                               {
                                                   if (safe->onMessage) safe->onMessage ("Saved as " + exportName (format) + ": " + made.getFileName() + ". It is shown in its folder.");
                                                   made.revealToUser();
                                               }
                                               else if (safe->onMessage && err != "stopped")
                                                   safe->onMessage ("Could not save the " + exportName (format) + ". " + err);
                                           });
}
} // namespace rh::ui
