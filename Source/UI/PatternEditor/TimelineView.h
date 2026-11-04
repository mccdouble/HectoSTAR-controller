/*
    ------------------------------------------------------------------

    Pattern editor timeline: one lane per LED in use, blocks drawn as
    rectangles with the rendered current (0..100 uA) traced over them.

    Mouse: click a block to select it (and its linked copies; Alt for just
    the one), Ctrl/Cmd or Shift to add to the selection. Drag to move, drag
    an edge to resize; moves snap to the ruler grid (hold Alt for single
    samples). Double-click an empty spot to add a block there.
    Wheel: scroll lanes; Shift+wheel: scroll time; Ctrl/Cmd+wheel: zoom
    time; Alt+wheel (or Ctrl+Shift+wheel): zoom lane height. Zoomed in
    far enough, the current is drawn as one step per 20 kHz sample (no
    interpolation) and hovering it shows that sample's value.
    Keys: Delete removes the selected blocks, Ctrl/Cmd+A selects all.

    ------------------------------------------------------------------
*/

#pragma once

#include "Playback.h"

namespace PatternEditor
{

class TimelineView : public Component,
                     public TooltipClient,
                     private ChangeListener,
                     private ScrollBar::Listener
{
public:
    explicit TimelineView (PatternDocument& doc);
    ~TimelineView() override;

    /** Shows 0 .. max (1 s, pattern length) across the width, and keeps
        doing so as the pattern changes until time is zoomed or scrolled by hand. */
    void zoomToFit();

    /** Makes the lanes as tall as fits (all lanes visible), and keeps them
        filling the height until the lane height is zoomed by hand. */
    void fitLanesToHeight();

    /** Shows the playhead of this playback (and scrolls to follow it). */
    void setPlayback (Playback* p);

    /** Called when a lane's name is clicked (toggles that LED's selection). */
    std::function<void (int led)> onLaneNameClicked;

    void paint (Graphics& g) override;

    /** Over a lane label: everything that LED plays (start, length, waveform). */
    String getTooltip() override;
    void resized() override;
    void mouseDown (const MouseEvent& e) override;
    void mouseDrag (const MouseEvent& e) override;
    void mouseUp (const MouseEvent& e) override;
    void mouseMove (const MouseEvent& e) override;
    void mouseDoubleClick (const MouseEvent& e) override;
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel) override;
    bool keyPressed (const KeyPress& key) override;

    static constexpr int kRulerHeight = 24;
    static constexpr int kLaneHeight = 44;     // smallest (default) lane height
    static constexpr int kMaxLaneHeight = 1200;
    static constexpr int kNameWidth = 230;
    static constexpr int kScrollSize = 12;

private:
    PatternDocument& doc;
    Playback* playback = nullptr;
    ScrollBar hScroll { false };
    ScrollBar vScroll { true };

    double pxPerSec = 600.0;
    Point<int> hoverPos;
    double viewStart = 0.0;
    double scrollY = 0.0;
    bool fitTime = true; // zoomToFit after every change until zoomed by hand
    void applyFitTime();
    int laneHeight = kLaneHeight;
    bool fillHeight = false; // fitLanesToHeight until zoomed by hand
    void applyFillHeight();

    /** Pixels per 20 kHz sample at the current zoom. */
    double pxPerSample() const;

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
