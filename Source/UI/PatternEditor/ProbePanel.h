/*
    ------------------------------------------------------------------

    Pattern editor probe map (4 shanks x 16 LEDs, same channel numbering as
    PatternGenGUI.m) and the sequencing bar that acts on the selection.

    Click an LED to select it, Ctrl/Cmd-click to toggle, drag across LEDs
    to add them. LEDs with blocks are tinted.

    Sequencing: "Add blocks" puts the new-block settings on every selected
    LED, offset in the chosen order by the step (linked, so they can be
    edited together); "Stagger" re-spaces the selected blocks the same way.

    ------------------------------------------------------------------
*/

#pragma once

#include "EditorStyle.h"
#include "PatternDocument.h"

namespace PatternEditor
{

enum class SequenceOrder
{
    Together,     // all start at once
    ChannelOrder, // CH1, CH2, ...
    BottomToTop,  // row by row up the shanks; same row starts together
    TopToBottom,
    ShankByShank  // shank 1, then 2, ...; a whole shank starts together
};

/** Rank (0, 1, 2, ...) of each LED in the order; equal ranks start together. */
std::vector<int> sequenceRanks (const std::vector<int>& leds, SequenceOrder order);

class ProbeMap : public Component,
                 private ChangeListener
{
public:
    explicit ProbeMap (PatternDocument& doc);
    ~ProbeMap() override;

    void paint (Graphics& g) override;
    void mouseDown (const MouseEvent& e) override;
    void mouseDrag (const MouseEvent& e) override;

private:
    PatternDocument& doc;
    bool dragToggle = false;

    void changeListenerCallback (ChangeBroadcaster*) override { repaint(); }
    juce::Rectangle<float> cellBounds (int led) const;
    int ledAt (Point<float> p) const;
};

/** Left column: selection buttons and the probe map. */
class ProbePanel : public Component,
                   private ChangeListener
{
public:
    explicit ProbePanel (PatternDocument& doc);
    ~ProbePanel() override;

    void resized() override;
    void paint (Graphics& g) override;

private:
    PatternDocument& doc;
    ProbeMap map;
    EditorButton selectAll { "All" }, selectNone { "None" }, selectInUse { "In use" };
    juce::Rectangle<int> header, hint;

    void changeListenerCallback (ChangeBroadcaster*) override { repaint (header); }
};

/** Order for a timing choice id (see addTimingChoices). */
SequenceOrder orderForTiming (int timingId);

/** Bar above the timeline: pick a waveform and a timing, then add it to the selected LEDs. */
class AddBar : public Component,
               private ChangeListener
{
public:
    explicit AddBar (PatternDocument& doc);
    ~AddBar() override;

    void resized() override;
    void paint (Graphics& g) override;

    /** One row when wide enough, otherwise two. */
    int preferredHeight (int width) const;

private:
    PatternDocument& doc;

    Label addLabel { {}, "Add" }, everyLabel { {}, "every" }, msLabel { {}, "ms" };
    ComboBox waveBox, timingBox;
    TextEditor stepEditor;
    EditorButton addButton { "Add", EditorButton::Style::Accent };
    std::unique_ptr<FileChooser> chooser;

    void changeListenerCallback (ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void chooseWave();
    void addBlocks();
    void addBlocksWith (const Block& prototype);
};

} // namespace PatternEditor
