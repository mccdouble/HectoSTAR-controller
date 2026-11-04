/*
    ------------------------------------------------------------------

    Small waveform views for the stim panel:

    WaveformPreview  plots one waveform as the DAC will output it (uA per
                     50 us sample), either one cycle or the whole block.
    ShapePalette     a grid of tiles, one per waveform choice, each with a
                     thumbnail of its shape; click a tile to choose it.

    ------------------------------------------------------------------
*/

#pragma once

#include "EditorStyle.h"
#include "WaveformCatalog.h"

namespace PatternEditor
{

class WaveformPreview : public Component
{
public:
    WaveformPreview();

    /** Shows this block's waveform (its start time is ignored). */
    void setBlock (const Block& b);

    /** One cycle (default) or the whole block. */
    void setWhole (bool whole);
    bool showsWhole() const { return whole; }

    /** Text drawn above the plot, e.g. "One cycle (100 ms)". */
    const String& getCaption() const { return caption; }

    void paint (Graphics& g) override;

private:
    Block block;
    bool whole = false;
    std::vector<double> samples;
    double length = 0;
    String caption;

    void recompute();
};

/** Draws samples scaled to fill area (0 at the bottom, the largest value at the top). */
void drawThumbnail (Graphics& g, juce::Rectangle<float> area, const std::vector<double>& samples, Colour colour, float thickness = 1.5f);

class ShapePalette : public Component,
                     public TooltipClient
{
public:
    ShapePalette();

    /** Shows these entries of waveChoices(). */
    void setChoices (std::vector<int> choiceIndices);

    /** Highlights the entry (index into waveChoices(); -1 for none). */
    void setSelected (int choiceIndex);

    std::function<void (int choiceIndex)> onChoose;

    int preferredHeight (int width) const;

    void paint (Graphics& g) override;
    void mouseDown (const MouseEvent& e) override;
    void mouseMove (const MouseEvent& e) override;
    void mouseExit (const MouseEvent& e) override;
    String getTooltip() override;

    static constexpr int kTileW = 92;
    static constexpr int kTileH = 62;
    static constexpr int kGap = 6;

private:
    std::vector<int> choices;
    std::vector<std::vector<double>> thumbs;
    int selected = -1;
    int hovered = -1;

    int columns (int width) const;
    juce::Rectangle<int> tileBounds (int i) const;
    int tileAt (Point<int> p) const;
};

} // namespace PatternEditor
