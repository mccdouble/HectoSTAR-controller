/*
    ------------------------------------------------------------------

    Pattern editor styling, matching the Open Ephys stim panel: solid
    ColourButtons (the same class the plugin's own buttons use) coloured
    from the current theme, and section headers.

    ------------------------------------------------------------------
*/

#pragma once

#include <VisualizerEditorHeaders.h>

namespace PatternEditor
{

class EditorButton : public ColourButton
{
public:
    enum class Style
    {
        Normal, // like "Select Pattern Stim File"
        Accent, // main actions, like "Upload Stim Parameters"
        Danger  // destructive
    };

    explicit EditorButton (const String& text, Style s = Style::Normal)
        : ColourButton (text, FontOptions (13.0f)), style (s)
    {
        applyColours();
    }

    /** Enables/disables the button and dims it when disabled. */
    void setActive (bool active)
    {
        setEnabled (active);
        setEnabledState (active);
    }

    void lookAndFeelChanged() override { applyColours(); }

private:
    Style style;

    void applyColours()
    {
        switch (style)
        {
            case Style::Normal:
                setColours (findColour (ThemeColours::defaultText), findColour (ThemeColours::defaultFill));
                break;
            case Style::Accent:
                setColours (findColour (ThemeColours::defaultText), findColour (ThemeColours::highlightedFill));
                break;
            case Style::Danger:
                setColours (Colours::white, Colour (0xffc0392b));
                break;
        }
        repaint();
    }
};

/** Draws a section title with a rule under it; returns the area below. */
inline juce::Rectangle<int> drawSectionHeader (Graphics& g, Component& c, juce::Rectangle<int> area, const String& title)
{
    auto header = area.removeFromTop (22);
    g.setColour (c.findColour (ThemeColours::defaultText));
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (title, header.withTrimmedLeft (2), Justification::centredLeft);
    g.setColour (c.findColour (ThemeColours::outline));
    g.drawHorizontalLine (header.getBottom() - 2, (float) header.getX(), (float) header.getRight());
    return area;
}

} // namespace PatternEditor
