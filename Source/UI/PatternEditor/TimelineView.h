/*
    ------------------------------------------------------------------

    Pattern editor timeline: one lane per LED in use, blocks drawn as
    rectangles with the rendered current (0..100 uA) traced over them.

    Mouse: click a block to select it (and its linked copies; Alt for just
    the one), Ctrl/Cmd or Shift to add to the selection. Drag to move, drag
    an edge to resize; moves snap to the ruler grid (hold Alt for single
    samples). Double-click an empty spot to add a block there.
    Wheel: scroll lanes; Shift+wheel: scroll time; Ctrl/Cmd+wheel: zoom.
    Keys: Delete removes the selected blocks, Ctrl/Cmd+A selects all.

    ------------------------------------------------------------------
*/

#pragma once

#include "PatternDocument.h"

namespace PatternEditor
{

class TimelineView : public Component,
                     private ChangeListener,
                     private ScrollBar::Listener
{
public:
    explicit TimelineView (PatternDocument& doc);
    ~TimelineView() override;

    /** Zooms so the whole pattern fits the width. */
    void zoomToFit();

    /** Called when a lane's name is clicked (toggles that LED's selection). */
    std::function<void (int led)> onLaneNameClicked;

    void paint (Graphics& g) override;
    void resized() override;
    void mouseDown (const MouseEvent& e) override;
    void mouseDrag (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    void mouseMove (const MouseEvent& e) override;
    void mouseDoubleClick (const MouseEvent& e) override;
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel) override;
    bool keyPressed (const KeyPress& key) override;

    static constexpr int kRulerHeight = 24;
    static constexpr int kLaneHeight = 44;
    static constexpr int kNameWidth = 78;
    static constexpr int kScrollSize = 12;

private:
    PatternDocument& doc;
    ScrollBar hScroll { false };
    ScrollBar vScroll { true };

    double pxPerSec = 600.0;
    double viewStart = 0.0;
    double scrollY = 0.0;

    enum class Drag
    {
        None,
        Move,
        ResizeStart,
        ResizeEnd
    };

    Drag drag = Drag::None;
    int dragPrimary = 0;
    double dragAnchorTime = 0;
    bool gestureStarted = false;
    struct Orig
    {
        int id;
        double start, length;
    };
    std::vector<Orig> dragOrig;

    void changeListenerCallback (ChangeBroadcaster*) override;
    void scrollBarMoved (ScrollBar* bar, double newRangeStart) override;
    void updateScrollBars();

    juce::Rectangle<int> lanesArea() const;
    float xForTime (double t) const;
    double timeForX (float x) const;
    int laneIndexAt (int y) const;
    int laneTop (int index) const;
    juce::Rectangle<float> blockRect (const Block& b, int laneIndex) const;
    double gridStep (double minPixels) const;
    double snapTime (double t, bool fine) const;

    struct Hit
    {
        int blockId = 0;
        Drag mode = Drag::None;
    };
    Hit hitTest (Point<float> p) const;

    void paintRuler (Graphics& g, juce::Rectangle<int> area);
    void paintLane (Graphics& g, int index, int led);
};

} // namespace PatternEditor
