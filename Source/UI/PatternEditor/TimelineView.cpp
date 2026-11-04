/*
    ------------------------------------------------------------------

    Pattern editor timeline. See TimelineView.h.

    ------------------------------------------------------------------
*/

#include "TimelineView.h"
#include "WaveformCatalog.h"

#include <algorithm>
#include <cmath>

namespace PatternEditor
{

namespace
{

String ledPlace (int led)
{
    const int row = rowFromBottom (led) + 1;
    return "shank " + String (shankOf (led)) + ", " + String (row) + (row == 1 ? "st" : row == 2 ? "nd" : row == 3 ? "rd" : "th") + " from tip";
}

std::vector<const Block*> blocksOn (const Pattern& p, int led)
{
    std::vector<const Block*> out;
    for (const auto& b : p.blocks)
        if (b.led == led)
            out.push_back (&b);
    std::sort (out.begin(), out.end(), [] (const Block* a, const Block* b) { return a->start < b->start; });
    return out;
}

String msText (double s)
{
    String t (s * 1000.0, 1);
    if (t.containsChar ('.'))
        t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return t + " ms";
}

} // namespace


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
    setPlayback (nullptr);
    doc.removeChangeListener (this);
}

void TimelineView::setPlayback (Playback* p)
{
    if (playback != nullptr)
        playback->removeChangeListener (this);
    playback = p;
    if (playback != nullptr)
        playback->addChangeListener (this);
}

void TimelineView::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == playback && playback != nullptr)
    {
        // keep the playhead in view
        if (playback->isActive())
        {
            const auto area = lanesArea();
            const double visible = area.getWidth() / pxPerSec;
            const double t = playback->getTime();
            if (t < viewStart || t > viewStart + visible)
            {
                viewStart = jmax (0.0, t - visible * 0.05);
                updateScrollBars();
            }
        }
        repaint();
        return;
    }

    applyFillHeight(); // the number of lanes may have changed
    applyFitTime();    // and the pattern length
    updateScrollBars();
    repaint();
}

double TimelineView::pxPerSample() const
{
    return pxPerSec / kSampleRateHz;
}

void TimelineView::fitLanesToHeight()
{
    fillHeight = true;
    scrollY = 0;
    applyFillHeight();
    updateScrollBars();
    repaint();
}

void TimelineView::applyFillHeight()
{
    if (! fillHeight)
        return;
    const int lanes = std::max (1, (int) doc.laneLeds().size());
    laneHeight = jlimit ((int) kLaneHeight, (int) kMaxLaneHeight, lanesArea().getHeight() / lanes);
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
    return kRulerHeight + index * laneHeight - (int) scrollY;
}

int TimelineView::laneIndexAt (int y) const
{
    if (y < kRulerHeight)
        return -1;
    const int index = (int) std::floor ((y - kRulerHeight + scrollY) / laneHeight);
    return (index >= 0 && index < (int) doc.laneLeds().size()) ? index : -1;
}

juce::Rectangle<float> TimelineView::blockRect (const Block& b, int laneIndex) const
{
    const float x0 = xForTime (b.start);
    const double end = std::isfinite (b.length) ? b.start + b.length : doc.pattern().duration;
    const float x1 = std::max (x0 + 4.0f, xForTime (end));
    const float top = (float) laneTop (laneIndex) + 3.0f;
    return { x0, top, x1 - x0, (float) laneHeight - 6.0f };
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
    fitTime = true;
    applyFitTime();
    updateScrollBars();
    repaint();
}

void TimelineView::applyFitTime()
{
    // not mid-drag: the view would shift under the mouse (refits on mouse-up)
    if (! fitTime || drag != Drag::None || lanesArea().getWidth() <= 50)
        return;
    // at least a second, so short patterns aren't stretched across the width
    const double length = std::max (1.0, doc.pattern().duration);
    pxPerSec = std::max (1.0, (lanesArea().getWidth() - 10) / length);
    viewStart = 0;
}

void TimelineView::updateScrollBars()
{
    const auto area = lanesArea();
    const double total = std::max (doc.pattern().duration * 1.05, viewStart + area.getWidth() / pxPerSec);
    hScroll.setRangeLimits (0.0, total, dontSendNotification);
    hScroll.setCurrentRange (viewStart, area.getWidth() / pxPerSec, dontSendNotification);

    const double content = (double) doc.laneLeds().size() * laneHeight;
    scrollY = jlimit (0.0, std::max (0.0, content - area.getHeight()), scrollY);
    vScroll.setRangeLimits (0.0, std::max (content, (double) area.getHeight()), dontSendNotification);
    vScroll.setCurrentRange (scrollY, area.getHeight(), dontSendNotification);
}

void TimelineView::scrollBarMoved (ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScroll)
    {
        viewStart = newRangeStart;
        fitTime = false;
    }
    else
        scrollY = newRangeStart;
    repaint();
}

void TimelineView::resized()
{
    auto b = getLocalBounds();
    hScroll.setBounds (b.removeFromBottom (kScrollSize).withTrimmedLeft (kNameWidth).withTrimmedRight (kScrollSize));
    vScroll.setBounds (b.removeFromRight (kScrollSize).withTrimmedTop (kRulerHeight));

    applyFitTime();
    applyFillHeight();
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
        if (top + laneHeight >= kRulerHeight && top < area.getBottom())
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

    if (playback != nullptr && playback->isActive())
    {
        const float x = xForTime (playback->getTime());
        if (x >= area.getX() && x <= area.getRight())
        {
            g.setColour (findColour (ThemeColours::highlightedFill));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, 0.0f, 2.0f, (float) area.getBottom()));
        }
    }

    if (leds.empty())
    {
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.6f));
        g.setFont (FontOptions (13.0f));
        g.drawText ("Select LEDs on the probe (click them, or type a range under it), choose a waveform above and click Apply. Each LED in the pattern gets a lane here.",
                    area.reduced (10, 4), Justification::centredLeft, true);
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
    const juce::Rectangle<int> lane (area.getX(), top, area.getWidth(), laneHeight);
    const juce::Rectangle<int> name (0, top, kNameWidth, laneHeight);

    const bool ledSelected = doc.selectedLeds().count (led) > 0;
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour accent = findColour (ThemeColours::highlightedFill);

    g.setColour (index % 2 == 0 ? findColour (ThemeColours::widgetBackground) : findColour (ThemeColours::componentBackground));
    g.fillRect (lane);

    g.setColour (ledSelected ? accent.withAlpha (0.35f) : findColour (ThemeColours::widgetBackground));
    g.fillRect (name);
    // label: LED and where it is, then what it plays (all of it in the tooltip)
    const auto blocks = blocksOn (doc.pattern(), led);
    auto rows = name.withHeight (kLaneHeight).reduced (8, 3); // the label keeps its size in tall lanes
    auto first = rows.removeFromTop (rows.getHeight() / 2);
    g.setColour (text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("LED " + String (led), first.removeFromLeft (58), Justification::centredLeft);
    g.setFont (FontOptions (11.0f));
    g.setColour (text.withAlpha (0.7f));
    g.drawText (ledPlace (led), first, Justification::centredLeft, true);

    String summary = "no waveform yet";
    if (! blocks.empty())
    {
        summary = describeWave (*blocks.front());
        if (blocks.size() > 1)
            summary = "+" + String ((int) blocks.size() - 1) + " more: " + summary;
    }
    g.setColour (text.withAlpha (blocks.empty() ? 0.5f : 0.9f));
    g.drawText (summary, rows, Justification::centredLeft, true);

    g.setColour (findColour (ThemeColours::outline).withAlpha (0.6f));
    g.drawHorizontalLine (top + laneHeight - 1, 0.0f, (float) lane.getRight());

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
        const float yBottom = (float) (top + laneHeight - 5);
        const float scale = (float) (laneHeight - 18) / (float) kMaxCurrentUa;

        // current grid in tall lanes, so the DAC's 1 uA steps can be read off
        {
            static const int steps[] = { 1, 2, 5, 10, 20, 50 };
            int step = 100;
            for (int s : steps)
                if (s * scale >= 6.0f)
                {
                    step = s;
                    break;
                }
            if (laneHeight >= 90)
            {
                g.setFont (FontOptions (10.0f));
                for (int v = 0; v <= kMaxCurrentUa; v += step)
                {
                    const float y = yBottom - v * scale;
                    const bool major = v % (step * 5) == 0 || v == kMaxCurrentUa;
                    g.setColour (text.withAlpha (major ? 0.18f : 0.07f));
                    g.drawHorizontalLine ((int) std::round (y), (float) lane.getX(), (float) lane.getRight());
                    if (major && scale * step * 5 >= 14.0f)
                    {
                        g.setColour (text.withAlpha (0.55f));
                        g.drawText (String (v) + " uA", lane.getX() + 3, (int) y - 12, 50, 11, Justification::left);
                    }
                }
            }
        }

        g.setColour (text.withAlpha (0.9f));

        if (pxPerSample() >= 2.0)
        {
            // zoomed in: one flat step per sample, exactly what the DAC outputs
            const int s0 = std::max (0, (int) std::floor (timeForX ((float) lane.getX()) * kSampleRateHz) - 1);
            const int s1 = std::min (data.numSamples, (int) std::ceil (timeForX ((float) lane.getRight()) * kSampleRateHz) + 1);
            Path steps;
            float prevY = 0;
            for (int s = s0; s < s1; ++s)
            {
                const float x0 = xForTime ((double) s / kSampleRateHz);
                const float x1 = xForTime ((double) (s + 1) / kSampleRateHz);
                const float y = yBottom - samples[s] * scale;
                if (s == s0)
                    steps.startNewSubPath (x0, y);
                else
                    steps.lineTo (x0, prevY), steps.lineTo (x0, y);
                steps.lineTo (x1, y);
                prevY = y;
            }
            g.strokePath (steps, PathStrokeType (1.5f));

            // sample instants
            if (pxPerSample() >= 8.0)
                for (int s = s0; s < s1; ++s)
                    g.fillEllipse (xForTime ((double) s / kSampleRateHz) - 2.0f, yBottom - samples[s] * scale - 2.0f, 4.0f, 4.0f);
        }
        else
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

String TimelineView::getTooltip()
{
    const int index = laneIndexAt (hoverPos.y);
    const auto leds = doc.laneLeds();
    if (index < 0 || index >= (int) leds.size())
        return {};

    if (hoverPos.x >= kNameWidth)
    {
        // zoomed in: the sample under the mouse
        const PatternData& data = doc.rendered();
        if (pxPerSample() < 2.0 || data.numSamples <= 0
            || data.current.size() != (std::size_t) kNumLeds * (std::size_t) data.numSamples)
            return {};
        const int s = (int) std::floor (timeForX ((float) hoverPos.x) * kSampleRateHz);
        if (s < 0 || s >= data.numSamples)
            return {};
        const int led = leds[(std::size_t) index];
        return "LED " + String (led) + ", sample " + String (s) + " (" + String (s * 1000.0 / kSampleRateHz, 2) + " ms): "
               + String ((int) data.led (led - 1)[s]) + " uA";
    }

    const int led = leds[(std::size_t) index];
    String tip = "LED " + String (led) + " (" + ledPlace (led) + "). Click to select it, Ctrl-click to add or remove it.";
    const auto blocks = blocksOn (doc.pattern(), led);
    if (blocks.empty())
        tip << "\nNo waveform yet.";
    for (const Block* b : blocks)
        tip << "\n" << msText (b->start) << " to " << msText (b->start + b->length) << ":  " << describeWave (*b);
    return tip;
}

void TimelineView::mouseMove (const MouseEvent& e)
{
    hoverPos = e.getPosition();
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
    if (fitTime)
        zoomToFit();
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
    if (e.mods.isAltDown() || (e.mods.isCommandDown() && e.mods.isShiftDown()))
    {
        // lane height, keeping the point under the mouse in place
        const double content = (e.position.y - kRulerHeight + scrollY) / laneHeight;
        fillHeight = false;
        laneHeight = jlimit ((int) kLaneHeight, (int) kMaxLaneHeight,
                             (int) std::round (laneHeight * std::exp (wheel.deltaY * 1.5)));
        scrollY = std::max (0.0, content * laneHeight - (e.position.y - kRulerHeight));
    }
    else if (e.mods.isCommandDown())
    {
        const double anchor = timeForX (e.position.x);
        const double factor = std::exp (wheel.deltaY * 1.5);
        pxPerSec = jlimit (1.0, 2.0e6, pxPerSec * factor);
        fitTime = false;
        viewStart = std::max (0.0, anchor - (e.position.x - kNameWidth) / pxPerSec);
    }
    else if (e.mods.isShiftDown() || std::fabs (wheel.deltaX) > std::fabs (wheel.deltaY))
    {
        const double d = (std::fabs (wheel.deltaX) > std::fabs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY);
        viewStart = std::max (0.0, viewStart - d * 200.0 / pxPerSec);
        fitTime = false;
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
