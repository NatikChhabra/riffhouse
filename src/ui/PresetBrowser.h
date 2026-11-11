// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Widgets.h"

namespace rh::ui
{
// Full-panel tone browser: search, categories, favourites, artists.
// Clicking a tone loads it right away so you can audition while playing.
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener
{
public:
    explicit PresetBrowser (RiffhouseProcessor&);
    ~PresetBrowser() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress&) override;
    void refreshList();
    std::function<void()> onClose, onLoaded;
    std::function<void (juce::String)> onMessage;

private:
    int getNumRows() override { return (int) results.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void selectedRowsChanged (int row) override;
    void textEditorTextChanged (juce::TextEditor&) override { refreshList(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { if (onClose) onClose(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override;

    class Categories;
    void load (int row);
    void pasteShareCode();
    void copyShareCode();

    RiffhouseProcessor& proc;
    juce::TextEditor search;
    std::unique_ptr<Categories> categories;
    juce::ListBox list { "Tones", this };
    IconButton close { icons::close() };
    juce::TextButton paste { "Paste tone code" }, copy { "Copy tone code" }, folder { "Open tone folder" };
    std::vector<int> results;
    juce::String category = "All";
    bool ignoreSelection = false;
};
} // namespace rh::ui
