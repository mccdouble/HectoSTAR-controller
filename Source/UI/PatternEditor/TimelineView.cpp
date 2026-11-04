/*
    ------------------------------------------------------------------

    Pattern editor timeline. See TimelineView.h.

    ------------------------------------------------------------------
*/

#include "TimelineView.h"
#include "WaveformCatalog.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

constexpr double kSamplePeriod = 1.0 / kSampleRateHz;
constexpr int kEdgeGrabPx = 6;

String timeLabel (double t, double step)
{
    if (step >= 1.0)
        return String (t, 0) + " s";
    if (step >= 0.1)
        return String (t, 1) + " s";
    if (step >= 0.001)
        return String (t * 1000.0, 0) + " ms";
    return String (t * 1000.0, 2) + " ms";
}

} // namespace

TimelineView::TimelineView (PatternDocument& d) : doc (d)
{
    setWantsKeyboardFocus (true);

    hScroll.setAutoHide (false);
    vScroll.setAutoHide (true);
    hScroll.addListener (this);
    vScroll.addListener (this);
    addAndMakeVisible (hScroll);
    addAndMakeVisible (vScroll);

    doc.addChangeListener (this);
}

TimelineView::~TimelineView()
{
    doc.removeChangeListener (this);
}

void TimelineView::changeListenerCallback (ChangeBroadcaster*)
{
    updateScrollBars();
    repaint();
}

juce::Rectangle<int> TimelineView::lanesArea() const
{
    return getLocalBounds()
        .withTrimmedTop (kRulerHeight)
        .withTrimmedLeft (kNameWidth)
        .withTrimmedBottom (kScrollSize)
        .withTrimmedRight (kScrollSize);
}

float TimelineView::xForTime (double t) const
{
    return (float) (kNameWidth + (t - viewStart) * pxPerSec);
}

double TimelineView::timeForX (float x) const
{
    return viewStart + (x - kNameWidth) / pxPerSec;
}

int TimelineView::laneTop (int index) const
{
    return kRulerHeight + index * kLaneHeight - (int) scrollY;
}

int TimelineView::laneIndexAt (int y) const
{
    if (y < kRulerHeight)
        return -1;
    const int index = (int) std::floor ((y - kRulerHeight + scrollY) / kLaneHeight);
    return (index >= 0 && index < (int) doc.laneLeds().size()) ? index : -1;
}

juce::Rectangle<float> TimelineView::blockRect (const Block& b, int laneIndex) const
{
    const float x0 = xForTime (b.start);
    const double end = std::isfinite (b.length) ? b.start + b.length : doc.pattern().duration;
    const float x1 = std::max (x0 + 4.0f, xForTime (end));
    const float top = (float) laneTop (laneIndex) + 3.0f;
    return { x0, top, x1 - x0, (float) kLaneHeight - 6.0f };
}

double TimelineView::gridStep (double minPixels) const
{
    static const double steps[] = { 0.00005, 0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
                                    0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0 };
    for (double s : steps)
        if (s * pxPerSec >= minPixels)
            return s;
    return 10.0;
}

double TimelineView::snapTime (double t, bool fine) const
{
    const double step = fine ? kSamplePeriod : std::max (kSamplePeriod, gridStep (8.0));
    return std::round (t / step) * step;
}

void TimelineView::zoomToFit()
{
    const double length = std::max (0.01, doc.pattern().duration);
    pxPerSec = std::max (1.0, (lanesArea().getWidth() - 10) / length);
    viewStart = 0;
    updateScrollBars();
    repaint();
}

void TimelineView::updateScrollBars()
{
    const auto area = lanesArea();
    const double total = std::max (doc.pattern().duration * 1.05, viewStart + area.getWidth() / pxPerSec);
    hScroll.setRangeLimits (0.0, total, dontSendNotification);
    hScroll.setCurrentRange (viewStart, area.getWidth() / pxPerSec, dontSendNotification);

    const double content = (double) doc.laneLeds().size() * kLaneHeight;
    scrollY = jlimit (0.0, std::max (0.0, content - area.getHeight()), scrollY);
    vScroll.setRangeLimits (0.0, std::max (content, (double) area.getHeight()), dontSendNotification);
    vScroll.setCurrentRange (scrollY, area.getHeight(), dontSendNotification);
}

void TimelineView::scrollBarMoved (ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScroll)
        viewStart = newRangeStart;
    else
        scrollY = newRangeStart;
    repaint();
}

void TimelineView::resized()
{
    auto b = getLocalBounds();
    hScroll.setBounds (b.removeFromBottom (kScrollSize).withTrimmedLeft (kNameWidth).withTrimmedRight (kScrollSize));
    vScroll.setBounds (b.removeFromRight (kScrollSize).withTrimmedTop (kRulerHeight));

    // show the whole pattern the first time there is room for it
    if (! fittedOnce && lanesArea().getWidth() > 50)
    {
        fittedOnce = true;
        zoomToFit();
        return;
    }
    updateScrollBars();
}

void TimelineView::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));

    const auto leds = doc.laneLeds();
    const auto area = lanesArea();

    g.saveState();
    g.reduceClipRegion (getLocalBounds().withTrimmedTop (kRulerHeight).withTrimmedBottom (kScrollSize).withTrimmedRight (kScrollSize));
    for (int i = 0; i < (int) leds.size(); ++i)
    {
        const int top = laneTop (i);
        if (top + kLaneHeight >= kRulerHeight && top < area.getBottom())
            paintLane (g, i, leds[(std::size_t) i]);
    }
    g.restoreState();

    // pattern end
    const float endX = xForTime (doc.pattern().duration);
    if (endX < area.getRight())
    {
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.08f));
        g.fillRect (juce::Rectangle<float> (std::max ((float) area.getX(), endX), (float) area.getY(), (float) area.getRight() - std::max ((float) area.getX(), endX), (float) area.getHeight()));
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.5f));
        g.drawVerticalLine ((int) endX, (float) area.getY(), (float) area.getBottom());
    }

    paintRuler (g, getLocalBounds().removeFromTop (kRulerHeight).withTrimmedLeft (kNameWidth).withTrimmedRight (kScrollSize));

    if (leds.empty())
    {
        // getting-started guide
        const auto box = area.withSizeKeepingCentre (jmin (520, area.getWidth() - 40), 150).toFloat();
        g.setColour (findColour (ThemeColours::widgetBackground));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (findColour (ThemeColours::outline));
        g.drawRoundedRectangle (box, 6.0f, 1.0f);

        auto lines = box.reduced (20, 14).toNearestInt();
        g.setColour (findColour (ThemeColours::defaultText));
        g.setFont (FontOptions (15.0f, Font::bold));
        g.drawText ("Build a pattern", lines.removeFromTop (26), Justification::centredLeft);
        g.setFont (FontOptions (13.0f));
        for (const char* step : { "1.  Select LEDs on the probe map (left).",
                                  "2.  Pick a pulse shape or preset above, and how the LEDs are timed.",
                                  "3.  Click \"Add\", then set amplitude, width, count and rate below.",
                                  "Or double-click a lane to add a single block." })
            g.drawText (step, lines.removeFromTop (24), Justification::centredLeft);
    }
}

void TimelineView::paintRuler (Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (findColour (ThemeColours::widgetBackground));
    g.fillRect (area);
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (area.getBottom() - 1, (float) area.getX(), (float) area.getRight());

    const double major = gridStep (70.0);
    const double minor = gridStep (10.0);
    const double t0 = std::floor (viewStart / minor) * minor;
    const double t1 = timeForX ((float) area.getRight());

    g.setFont (FontOptions (11.0f));
    for (double t = t0; t <= t1; t += minor)
    {
        const float x = xForTime (t);
        if (x < area.getX())
            continue;
        const bool isMajor = std::fabs (std::remainder (t, major)) < minor * 0.01;
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (isMajor ? 0.8f : 0.35f));
        g.drawVerticalLine ((int) x, (float) (area.getBottom() - (isMajor ? 10 : 5)), (float) area.getBottom());
        if (isMajor)
            g.drawText (timeLabel (std::round (t / minor) * minor, major), (int) x + 3, area.getY() + 1, 70, 14, Justification::left);
    }
}

void TimelineView::paintLane (Graphics& g, int index, int led)
{
    const int top = laneTop (index);
    const auto area = lanesArea();
    const juce::Rectangle<int> lane (area.getX(), top, area.getWidth(), kLaneHeight);
    const juce::Rectangle<int> name (0, top, kNameWidth, kLaneHeight);

    const bool ledSelected = doc.selectedLeds().count (led) > 0;
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour accent = findColour (ThemeColours::highlightedFill);

    g.setColour (index % 2 == 0 ? findColour (ThemeColours::widgetBackground) : findColour (ThemeColours::componentBackground));
    g.fillRect (lane);

    g.setColour (ledSelected ? accent.withAlpha (0.35f) : findColour (ThemeColours::widgetBackground));
    g.fillRect (name);
    g.setColour (text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("CH " + String (led), name.reduced (6, 0).withTrimmedBottom (kLaneHeight / 2), Justification::bottomLeft);
    g.setFont (FontOptions (11.0f));
    g.setColour (text.withAlpha (0.7f));
    g.drawText ("S" + String (shankOf (led)) + " row " + String (rowFromBottom (led) + 1), name.reduced (6, 0).withTrimmedTop (kLaneHeight / 2), Justification::topLeft);

    g.setColour (findColour (ThemeColours::outline).withAlpha (0.6f));
    g.drawHorizontalLine (top + kLaneHeight - 1, 0.0f, (float) lane.getRight());

    g.saveState();
    g.reduceClipRegion (lane);

    // blocks
    for (const Block& b : doc.pattern().blocks)
    {
        if (b.led != led)
            continue;
        const auto r = blockRect (b, index);
        const bool sel = doc.isBlockSelected (b.id);
        g.setColour (accent.withAlpha (sel ? 0.45f : 0.2f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (sel ? accent : accent.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, sel ? 2.0f : 1.0f);

        String label = b.label.empty() ? waveDisplayName (b.wave) : String::fromUTF8 (b.label.c_str());
        if (b.link != 0)
            label << "  (linked)";
        g.setColour (text.withAlpha (0.85f));
        g.setFont (FontOptions (11.0f));
        g.drawText (label, r.reduced (5, 1).toNearestInt(), Justification::topLeft, true);
    }

    // rendered current for this LED (what is actually written to the file)
    const PatternData& data = doc.rendered();
    if (data.numSamples > 0 && data.current.size() == (std::size_t) kNumLeds * (std::size_t) data.numSamples)
    {
        const std::uint8_t* samples = data.led (led - 1);
        const float yBottom = (float) (top + kLaneHeight - 5);
        const float scale = (float) (kLaneHeight - 18) / (float) kMaxCurrentUa;

        g.setColour (text.withAlpha (0.9f));

        for (int x = lane.getX(); x < lane.getRight(); ++x)
        {
            const double ta = timeForX ((float) x);
            const double tb = timeForX ((float) x + 1.0f);
            int s0 = (int) std::ceil (ta * kSampleRateHz);
            int s1 = (int) std::ceil (tb * kSampleRateHz);
            if (s1 <= s0)
                s1 = s0 + 1;
            if (s1 <= 0 || s0 >= data.numSamples)
                continue;
            s0 = std::max (s0, 0);
            s1 = std::min (s1, data.numSamples);

            std::uint8_t lo = 255, hi = 0;
            for (int s = s0; s < s1; ++s)
            {
                lo = std::min (lo, samples[s]);
                hi = std::max (hi, samples[s]);
            }
            if (hi == 0)
                continue;
            g.drawVerticalLine (x, yBottom - hi * scale - 1.0f, yBottom - lo * scale + 1.0f);
        }
    }

    g.restoreState();
}

TimelineView::Hit TimelineView::hitTest (Point<float> p) const
{
    Hit hit;
    const int index = laneIndexAt ((int) p.y);
    if (index < 0 || p.x < kNameWidth)
        return hit;

    const int led = doc.laneLeds()[(std::size_t) index];

    // last drawn (topmost) wins
    for (auto it = doc.pattern().blocks.rbegin(); it != doc.pattern().blocks.rend(); ++it)
    {
        if (it->led != led)
            continue;
        const auto r = blockRect (*it, index);
        if (! r.contains (p))
            continue;

        hit.blockId = it->id;
        if (r.getWidth() > 3 * kEdgeGrabPx && p.x > r.getRight() - kEdgeGrabPx)
            hit.mode = Drag::ResizeEnd;
        else if (r.getWidth() > 3 * kEdgeGrabPx && p.x < r.getX() + kEdgeGrabPx)
            hit.mode = Drag::ResizeStart;
        else
            hit.mode = Drag::Move;
        return hit;
    }

    return hit;
}

void TimelineView::mouseMove (const MouseEvent& e)
{
    const auto hit = hitTest (e.position);
    setMouseCursor (hit.mode == Drag::ResizeEnd || hit.mode == Drag::ResizeStart ? MouseCursor::LeftRightResizeCursor
                                                                                 : MouseCursor::NormalCursor);
}

void TimelineView::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();
    drag = Drag::None;

    const int index = laneIndexAt (e.y);

    if (index >= 0 && e.x < kNameWidth)
    {
        if (onLaneNameClicked)
            onLaneNameClicked (doc.laneLeds()[(std::size_t) index]);
        return;
    }

    const auto hit = hitTest (e.position);
    const bool add = e.mods.isCommandDown() || e.mods.isShiftDown();

    if (hit.blockId == 0)
    {
        if (! add)
            doc.setSelectedBlocks ({});
        return;
    }

    const std::set<int> group = e.mods.isAltDown() ? std::set<int> { hit.blockId } : doc.linkGroup (hit.blockId);
    std::set<int> sel = doc.selectedBlocks();

    if (e.mods.isCommandDown())
    {
        const bool allIn = std::all_of (group.begin(), group.end(), [&] (int id) { return sel.count (id) > 0; });
        for (int id : group)
            allIn ? (void) sel.erase (id) : (void) sel.insert (id);
    }
    else if (e.mods.isShiftDown() || doc.isBlockSelected (hit.blockId))
    {
        sel.insert (group.begin(), group.end());
    }
    else
    {
        sel = group;
    }
    doc.setSelectedBlocks (sel);

    if (! doc.isBlockSelected (hit.blockId))
        return;

    drag = hit.mode;
    dragPrimary = hit.blockId;
    dragAnchorTime = timeForX (e.position.x);
    gestureStarted = false;
    dragOrig.clear();
    for (int id : doc.selectedBlocks())
        if (const Block* b = doc.findBlock (id))
            dragOrig.push_back ({ id, b->start, b->length });
}

void TimelineView::mouseDrag (const MouseEvent& e)
{
    if (drag == Drag::None || dragOrig.empty())
        return;

    const bool fine = e.mods.isAltDown();
    const double raw = timeForX (e.position.x) - dragAnchorTime;

    const Orig* primary = nullptr;
    for (const auto& o : dragOrig)
        if (o.id == dragPrimary)
            primary = &o;
    if (primary == nullptr)
        return;

    // snap the edge being dragged on the primary block, move the rest by the same amount
    double delta;
    if (drag == Drag::ResizeEnd)
        delta = snapTime (primary->start + primary->length + raw, fine) - (primary->start + primary->length);
    else
        delta = snapTime (primary->start + raw, fine) - primary->start;

    if (! gestureStarted)
    {
        if (std::fabs (delta) < kSamplePeriod * 0.5)
            return;
        doc.beginGesture();
        gestureStarted = true;
    }

    const Drag mode = drag;
    const auto orig = dragOrig;
    doc.modifyLive ([&] (Pattern& p)
                    {
                        for (auto& b : p.blocks)
                        {
                            for (const auto& o : orig)
                            {
                                if (o.id != b.id)
                                    continue;
                                if (mode == Drag::Move)
                                {
                                    b.start = std::max (0.0, o.start + delta);
                                }
                                else if (mode == Drag::ResizeEnd)
                                {
                                    b.length = std::max (kSamplePeriod, o.length + delta);
                                }
                                else
                                {
                                    const double end = o.start + o.length;
                                    b.start = jlimit (0.0, end - kSamplePeriod, o.start + delta);
                                    b.length = end - b.start;
                                }
                            }
                        }
                    });
}

void TimelineView::mouseUp (const MouseEvent&)
{
    drag = Drag::None;
    dragOrig.clear();
}

void TimelineView::mouseDoubleClick (const MouseEvent& e)
{
    if (e.y < kRulerHeight)
    {
        zoomToFit();
        return;
    }

    const int index = laneIndexAt (e.y);
    if (index < 0 || e.x < kNameWidth || hitTest (e.position).blockId != 0)
        return;

    const int led = doc.laneLeds()[(std::size_t) index];
    const double t = std::max (0.0, snapTime (timeForX (e.position.x), e.mods.isAltDown()));

    int newId = 0;
    doc.modify ([&] (Pattern& p)
                {
                    Block b = doc.newBlockTemplate;
                    b.id = newId = nextBlockId (p);
                    b.led = led;
                    b.start = t;
                    b.link = 0;
                    if (naturalLength (b.wave) <= 0)
                        b.length = std::max (1.0 / kSampleRateHz, p.duration - t);
                    p.blocks.push_back (b);
                });
    doc.setSelectedBlocks ({ newId });
}

void TimelineView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        const double anchor = timeForX (e.position.x);
        const double factor = std::exp (wheel.deltaY * 1.5);
        pxPerSec = jlimit (1.0, 2.0e6, pxPerSec * factor);
        viewStart = std::max (0.0, anchor - (e.position.x - kNameWidth) / pxPerSec);
    }
    else if (e.mods.isShiftDown() || std::fabs (wheel.deltaX) > std::fabs (wheel.deltaY))
    {
        const double d = (std::fabs (wheel.deltaX) > std::fabs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY);
        viewStart = std::max (0.0, viewStart - d * 200.0 / pxPerSec);
    }
    else
    {
        scrollY -= wheel.deltaY * 150.0;
    }

    updateScrollBars();
    repaint();
}

bool TimelineView::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
    {
        if (doc.selectedBlocks().empty())
            return false;
        const auto sel = doc.selectedBlocks();
        doc.modify ([&] (Pattern& p)
                    { p.blocks.erase (std::remove_if (p.blocks.begin(), p.blocks.end(), [&] (const Block& b)
                                                      { return sel.count (b.id) > 0; }),
                                      p.blocks.end()); });
        return true;
    }

    if (key == KeyPress ('a', ModifierKeys::commandModifier, 0))
    {
        std::set<int> all;
        for (const auto& b : doc.pattern().blocks)
            all.insert (b.id);
        doc.setSelectedBlocks (all);
        return true;
    }

    return false;
}

} // namespace PatternEditor
