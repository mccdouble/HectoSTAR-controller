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

    /** Enables/disables the button. Disabled buttons are faded rather than
        darkened (ColourButton's own disabled look is nearly black). */
    void setActive (bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;
        active = shouldBeActive;
        setEnabled (active);
        applyColours();
    }

    void lookAndFeelChanged() override { applyColours(); }

private:
    Style style;
    bool active = true;

    void applyColours()
    {
        Colour text = findColour (ThemeColours::defaultText);
        Colour fill = findColour (ThemeColours::defaultFill);

        if (style == Style::Accent)
            fill = findColour (ThemeColours::highlightedFill);
        else if (style == Style::Danger)
        {
            text = Colours::white;
            fill = Colour (0xffc0392b);
        }

        if (! active)
        {
            fill = fill.interpolatedWith (findColour (ThemeColours::componentBackground), 0.6f);
            text = text.withAlpha (0.4f);
        }

        setEnabledState (true); // keep ColourButton from darkening it
        setColours (text, fill);
        repaint();
    }
};

/** Width of a line of text in the editor's label font. */
inline int textWidth (const String& text, float size = 13.0f)
{
    return (int) std::ceil (GlyphArrangement::getStringWidth (Font (FontOptions (size)), text));
}

/** Draws a section title with a rule under it. */
inline void drawSectionHeader (Graphics& g, Component& c, juce::Rectangle<int> header, const String& title)
{
    g.setColour (c.findColour (ThemeColours::defaultText));
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText (title, header.withTrimmedLeft (2), Justification::centredLeft);
    g.setColour (c.findColour (ThemeColours::outline));
    g.drawHorizontalLine (header.getBottom() - 2, (float) header.getX(), (float) header.getRight());
}

} // namespace PatternEditor
