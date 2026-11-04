/*
    ------------------------------------------------------------------

    Waveform preview and shape palette. See WaveformViews.h.

    ------------------------------------------------------------------
*/

#include "WaveformViews.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

String msLabel (double s)
{
    const double ms = s * 1000.0;
    String t (ms, ms < 10 ? 2 : (ms < 100 ? 1 : 0));
    if (t.containsChar ('.'))
        t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return t + " ms";
}

/** Path through the samples, stepped like the DAC output; columns with many
    samples are drawn as their min..max range. */
Path tracePath (const std::vector<double>& samples, juce::Rectangle<float> area, double top)
{
    Path p;
    const int n = (int) samples.size();
    if (n == 0 || top <= 0)
        return p;

    auto yFor = [&] (double v) { return area.getBottom() - (float) (jlimit (0.0, top, v) / top) * area.getHeight(); };
    const float w = area.getWidth();

    if (n <= (int) w * 2)
    {
        // sample-and-hold steps
        p.startNewSubPath (area.getX(), yFor (samples[0]));
        for (int i = 0; i < n; ++i)
        {
            const float x0 = area.getX() + w * (float) i / n;
            const float x1 = area.getX() + w * (float) (i + 1) / n;
            p.lineTo (x0, yFor (samples[(std::size_t) i]));
            p.lineTo (x1, yFor (samples[(std::size_t) i]));
        }
        return p;
    }

    // min/max per pixel column
    const int cols = jmax (1, (int) w);
    bool started = false;
    for (int c = 0; c < cols; ++c)
    {
        const int a = (int) ((long long) c * n / cols);
        const int b = jmax (a + 1, (int) ((long long) (c + 1) * n / cols));
        double lo = samples[(std::size_t) a], hi = lo;
        for (int i = a; i < b && i < n; ++i)
        {
            lo = jmin (lo, samples[(std::size_t) i]);
            hi = jmax (hi, samples[(std::size_t) i]);
        }
        const float x = area.getX() + (float) c + 0.5f;
        if (! started)
        {
            p.startNewSubPath (x, yFor (lo));
            started = true;
        }
        else
            p.lineTo (x, yFor (lo));
        p.lineTo (x, yFor (hi));
    }
    return p;
}

} // namespace

//==============================================================================

WaveformPreview::WaveformPreview()
{
    setInterceptsMouseClicks (false, false);
}

void WaveformPreview::setBlock (const Block& b)
{
    block = b;
    fixedLength = 0;
    recompute();
}

void WaveformPreview::setBlock (const Block& b, double len, const String& title)
{
    block = b;
    fixedLength = len;
    fixedTitle = title;
    recompute();
}

void WaveformPreview::setWhole (bool w)
{
    if (whole == w)
        return;
    whole = w;
    recompute();
}

void WaveformPreview::recompute()
{
    if (fixedLength > 0)
    {
        length = fixedLength;
        caption = fixedTitle + " (" + msLabel (length) + ")";
    }
    else if (whole)
    {
        const double natural = naturalLength (block.wave);
        length = natural > 0 ? natural : (block.length > 0 ? block.length : 1.0);
        caption = "Whole block (" + msLabel (length) + ")";
    }
    else
    {
        length = cycleLength (block);
        caption = "One cycle (" + msLabel (length) + ")";
    }

    length = jlimit (1.0 / kSampleRateHz, 13.0, length);
    samples = renderAlone (block, length);
    repaint();
}

void WaveformPreview::paint (Graphics& g)
{
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour trace = findColour (ThemeColours::highlightedFill);

    auto bounds = getLocalBounds().toFloat();
    g.setColour (findColour (ThemeColours::widgetBackground));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (findColour (ThemeColours::outline));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    auto plot = bounds.reduced (8.0f);
    plot.removeFromTop (16.0f); // caption
    plot.removeFromLeft (40.0f); // uA labels
    plot.removeFromBottom (16.0f); // time labels

    g.setColour (text);
    g.setFont (FontOptions (12.0f, Font::bold));
    g.drawText (caption, bounds.reduced (8.0f).removeFromTop (16.0f).toNearestInt(), Justification::centredLeft);

    double peak = 0;
    for (double v : samples)
        peak = jmax (peak, v);

    // scale: a round number at or above the peak
    double top = 10;
    for (double t : { 10.0, 20.0, 25.0, 50.0, 75.0, 100.0 })
    {
        top = t;
        if (peak <= t)
            break;
    }

    g.setFont (FontOptions (11.0f));
    for (double v : { 0.0, top / 2.0, top })
    {
        const float y = plot.getBottom() - (float) (v / top) * plot.getHeight();
        g.setColour (findColour (ThemeColours::outline).withAlpha (0.6f));
        g.drawHorizontalLine ((int) std::round (y), plot.getX(), plot.getRight());
        g.setColour (text.withAlpha (0.7f));
        g.drawText (String (v, 0) + " " + kMicroAmps, juce::Rectangle<float> (bounds.getX() + 2, y - 7, 44, 14).toNearestInt(), Justification::centredRight);
    }

    g.setColour (text.withAlpha (0.7f));
    const auto timeRow = juce::Rectangle<float> (plot.getX(), plot.getBottom() + 2, plot.getWidth(), 14).toNearestInt();
    g.drawText ("0", timeRow, Justification::centredLeft);
    g.drawText (msLabel (length / 2), timeRow, Justification::centred);
    g.drawText (msLabel (length), timeRow, Justification::centredRight);

    if (samples.empty())
    {
        g.drawText ("Nothing to show", plot.toNearestInt(), Justification::centred);
        return;
    }

    g.setColour (trace);
    g.strokePath (tracePath (samples, plot, top), PathStrokeType (1.6f));

    g.setColour (text.withAlpha (0.8f));
    g.drawText ("peak " + String (peak, 0) + " " + kMicroAmps, plot.toNearestInt().removeFromTop (14), Justification::topRight);
}

//==============================================================================

void drawThumbnail (Graphics& g, juce::Rectangle<float> area, const std::vector<double>& samples, Colour colour, float thickness)
{
    double peak = 0;
    for (double v : samples)
        peak = jmax (peak, v);
    if (peak <= 0)
        return;
    g.setColour (colour);
    g.strokePath (tracePath (samples, area, peak), PathStrokeType (thickness));
}

ShapePalette::ShapePalette()
{
    setRepaintsOnMouseActivity (false);
}

void ShapePalette::setChoices (std::vector<int> indices)
{
    choices = std::move (indices);
    thumbs.clear();

    const auto& list = waveChoices();
    for (int idx : choices)
    {
        Block b;
        b.wave = list[(std::size_t) idx].wave;

        // one pulse for shapes, one cycle otherwise
        double len;
        if (b.wave.kind == WaveKind::Shaped)
        {
            b.wave.shaped.count = 1;
            b.wave.shaped.width = 0.01;
            b.wave.shaped.equalCharge = false;
            b.wave.shaped.amp = 100;
            len = 0.0105;
        }
        else if (b.wave.kind == WaveKind::Formula)
        {
            b.length = 0.25;
            len = 0.25;
        }
        else
        {
            b.length = 1.0;
            len = cycleLength (b);
        }

        thumbs.push_back (b.wave.kind == WaveKind::Samples ? std::vector<double>() : renderAlone (b, len));
    }

    hovered = -1;
    repaint();
}

void ShapePalette::setSelected (int idx)
{
    if (selected != idx)
    {
        selected = idx;
        repaint();
    }
}

int ShapePalette::columns (int width) const
{
    return jmax (1, (width + kGap) / (kTileW + kGap));
}

int ShapePalette::preferredHeight (int width) const
{
    const int cols = columns (width);
    const int rows = jmax (1, ((int) choices.size() + cols - 1) / cols);
    return rows * kTileH + (rows - 1) * kGap;
}

juce::Rectangle<int> ShapePalette::tileBounds (int i) const
{
    const int cols = columns (getWidth());
    return { (i % cols) * (kTileW + kGap), (i / cols) * (kTileH + kGap), kTileW, kTileH };
}

int ShapePalette::tileAt (Point<int> p) const
{
    for (int i = 0; i < (int) choices.size(); ++i)
        if (tileBounds (i).contains (p))
            return i;
    return -1;
}

void ShapePalette::paint (Graphics& g)
{
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour accent = findColour (ThemeColours::highlightedFill);
    const auto& list = waveChoices();

    for (int i = 0; i < (int) choices.size(); ++i)
    {
        const auto r = tileBounds (i).toFloat();
        const bool isSel = choices[(std::size_t) i] == selected;

        g.setColour (isSel ? accent.withAlpha (0.25f) : findColour (ThemeColours::widgetBackground));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (isSel ? accent : (i == hovered ? text.withAlpha (0.6f) : findColour (ThemeColours::outline)));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, isSel ? 2.0f : 1.0f);

        auto inner = r.reduced (6.0f, 4.0f);
        auto thumbArea = inner.removeFromTop (28.0f);
        const auto& th = thumbs[(std::size_t) i];
        if (th.empty())
        {
            g.setColour (text.withAlpha (0.7f));
            g.setFont (FontOptions (12.0f, Font::bold));
            g.drawText ("CSV", thumbArea.toNearestInt(), Justification::centred);
        }
        else
        {
            g.setColour (findColour (ThemeColours::outline).withAlpha (0.5f));
            g.drawHorizontalLine ((int) thumbArea.getBottom(), thumbArea.getX(), thumbArea.getRight());
            drawThumbnail (g, thumbArea, th, isSel ? accent : text.withAlpha (0.85f));
        }

        String name = list[(std::size_t) choices[(std::size_t) i]].name;
        if (name.startsWith ("Custom: "))
            name = name.fromFirstOccurrenceOf ("Custom: ", false, false);
        g.setColour (text);
        g.setFont (FontOptions (11.0f));
        g.drawFittedText (name, inner.toNearestInt(), Justification::centred, 2, 0.9f);
    }
}

void ShapePalette::mouseDown (const MouseEvent& e)
{
    const int i = tileAt (e.getPosition());
    if (i >= 0 && onChoose)
        onChoose (choices[(std::size_t) i]);
}

void ShapePalette::mouseMove (const MouseEvent& e)
{
    const int i = tileAt (e.getPosition());
    if (i != hovered)
    {
        hovered = i;
        repaint();
    }
}

void ShapePalette::mouseExit (const MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

String ShapePalette::getTooltip()
{
    if (hovered < 0 || hovered >= (int) choices.size())
        return {};
    const auto& c = waveChoices()[(std::size_t) choices[(std::size_t) hovered]];
    if (c.wave.kind == WaveKind::Shaped)
        return c.name + ": " + shapeGuide (c.wave.shaped.shape).upToFirstOccurrenceOf ("  Equal charge", false, false);
    if (c.wave.kind == WaveKind::Builtin)
        return c.name + ": " + typeGuide (c.wave.builtin.type);
    return c.name;
}

} // namespace PatternEditor
