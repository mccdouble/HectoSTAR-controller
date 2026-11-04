/*
    ------------------------------------------------------------------

    Pattern editor probe map (4 shanks x 16 LEDs, same channel numbering as
    PatternGenGUI.m) and the sequencing tools that act on the selection.

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
    std::set<int> dragBase;
    bool dragToggle = false;

    void changeListenerCallback (ChangeBroadcaster*) override { repaint(); }
    juce::Rectangle<float> cellBounds (int led) const;
    int ledAt (Point<float> p) const;
};

class ProbePanel : public Component
{
public:
    explicit ProbePanel (PatternDocument& doc);

    void resized() override;
    void paint (Graphics& g) override;

private:
    PatternDocument& doc;
    ProbeMap map;

    EditorButton selectAll { "All" }, selectNone { "None" }, selectInUse { "In use" };
    Label orderLabel { {}, "Order" }, stepLabel { {}, "Step (ms)" }, startLabel { {}, "Start (ms)" };
    ComboBox orderBox;
    TextEditor stepEditor, startEditor;
    EditorButton addButton { "Add blocks to selected LEDs", EditorButton::Style::Accent };
    EditorButton staggerButton { "Stagger selected blocks" };

    juce::Rectangle<int> ledsHeader, sequenceHeader, hintArea;

    SequenceOrder order() const;
    double stepSeconds() const;

    void addBlocks();
    void staggerBlocks();
};

} // namespace PatternEditor
