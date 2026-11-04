/*
    ------------------------------------------------------------------

    Pattern editor probe map and sequencing bar. See ProbePanel.h.

    ------------------------------------------------------------------
*/

#include "ProbePanel.h"

#include <algorithm>
#include <map>

namespace PatternEditor
{

std::vector<int> sequenceRanks (const std::vector<int>& leds, SequenceOrder order)
{
    // key per LED; equal keys share a rank, ranks are dense in key order
    std::vector<int> keys;
    for (const int led : leds)
    {
        switch (order)
        {
            case SequenceOrder::Together: keys.push_back (0); break;
            case SequenceOrder::ChannelOrder: keys.push_back (led); break;
            case SequenceOrder::BottomToTop: keys.push_back (rowFromBottom (led)); break;
            case SequenceOrder::TopToBottom: keys.push_back (15 - rowFromBottom (led)); break;
            case SequenceOrder::ShankByShank: keys.push_back (shankOf (led)); break;
        }
    }

    std::vector<int> sorted (keys);
    std::sort (sorted.begin(), sorted.end());
    sorted.erase (std::unique (sorted.begin(), sorted.end()), sorted.end());

    std::vector<int> ranks;
    for (int k : keys)
        ranks.push_back ((int) (std::lower_bound (sorted.begin(), sorted.end(), k) - sorted.begin()));
    return ranks;
}

//==============================================================================

ProbeMap::ProbeMap (PatternDocument& d) : doc (d)
{
    doc.addChangeListener (this);
}

ProbeMap::~ProbeMap()
{
    doc.removeChangeListener (this);
}

juce::Rectangle<float> ProbeMap::cellBounds (int led) const
{
    const float header = 20.0f;
    const float colW = getWidth() / 4.0f;
    const float rowH = (getHeight() - header) / 16.0f;
    const int col = shankOf (led) - 1;
    const int rowFromTop = 15 - rowFromBottom (led);
    return { col * colW + 4.0f, header + rowFromTop * rowH + 2.0f, colW - 8.0f, rowH - 4.0f };
}

int ProbeMap::ledAt (Point<float> p) const
{
    for (int led = 1; led <= kNumLeds; ++led)
        if (cellBounds (led).expanded (2.0f).contains (p))
            return led;
    return 0;
}

void ProbeMap::paint (Graphics& g)
{
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour accent = findColour (ThemeColours::highlightedFill);

    std::set<int> used;
    for (int led : activeLeds (doc.pattern()))
        used.insert (led);

    // shank outlines behind the LEDs
    const float colW = getWidth() / 4.0f;
    for (int s = 0; s < 4; ++s)
    {
        const juce::Rectangle<float> shank (s * colW + 1.0f, 20.0f, colW - 2.0f, getHeight() - 21.0f);
        g.setColour (findColour (ThemeColours::widgetBackground));
        g.fillRoundedRectangle (shank, 4.0f);

        g.setColour (text.withAlpha (0.8f));
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText ("S" + String (s + 1), juce::Rectangle<float> (s * colW, 0, colW, 18.0f), Justification::centred);
    }

    const float rowH = (getHeight() - 20.0f) / 16.0f;
    g.setFont (FontOptions (jlimit (10.0f, 13.0f, rowH * 0.55f)));

    for (int led = 1; led <= kNumLeds; ++led)
    {
        const auto r = cellBounds (led);
        const bool sel = doc.selectedLeds().count (led) > 0;
        const bool inUse = used.count (led) > 0;

        g.setColour (sel ? accent : (inUse ? accent.withAlpha (0.35f) : findColour (ThemeColours::componentBackground)));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (sel ? accent.darker (0.3f) : findColour (ThemeColours::outline));
        g.drawRoundedRectangle (r, 3.0f, sel ? 1.5f : 0.75f);

        g.setColour (sel ? findColour (ThemeColours::highlightedText) : text);
        g.drawText (String (led), r, Justification::centred);
    }
}

void ProbeMap::mouseDown (const MouseEvent& e)
{
    const int led = ledAt (e.position);
    dragToggle = e.mods.isCommandDown();

    if (led == 0)
    {
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            doc.setSelectedLeds ({});
        return;
    }

    std::set<int> sel = (e.mods.isCommandDown() || e.mods.isShiftDown()) ? doc.selectedLeds() : std::set<int>();

    if (dragToggle && sel.count (led) > 0)
        sel.erase (led);
    else
        sel.insert (led);

    doc.setSelectedLeds (sel);
}

void ProbeMap::mouseDrag (const MouseEvent& e)
{
    const int led = ledAt (e.position);
    if (led == 0 || doc.selectedLeds().count (led) > 0)
        return;

    std::set<int> sel = doc.selectedLeds();
    sel.insert (led);
    doc.setSelectedLeds (sel);
}

//==============================================================================

ProbePanel::ProbePanel (PatternDocument& d) : doc (d), map (d)
{
    addAndMakeVisible (map);

    for (auto* b : { &selectAll, &selectNone, &selectInUse })
        addAndMakeVisible (b);

    selectAll.onClick = [this]
    {
        std::set<int> all;
        for (int led = 1; led <= kNumLeds; ++led)
            all.insert (led);
        doc.setSelectedLeds (all);
    };
    selectNone.onClick = [this] { doc.setSelectedLeds ({}); };
    selectInUse.onClick = [this]
    {
        const auto leds = activeLeds (doc.pattern());
        doc.setSelectedLeds (std::set<int> (leds.begin(), leds.end()));
    };
    selectInUse.setTooltip ("Select the LEDs that have blocks");

    doc.addChangeListener (this);
}

ProbePanel::~ProbePanel()
{
    doc.removeChangeListener (this);
}

void ProbePanel::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));

    const auto n = doc.selectedLeds().size();
    drawSectionHeader (g, *this, header, n == 0 ? String ("Probe LEDs") : "Probe LEDs  (" + String ((int) n) + " selected)");

    g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.65f));
    g.setFont (FontOptions (12.0f));
    g.drawFittedText ("Click, Ctrl-click or drag to select. Tinted LEDs have blocks.", hint, Justification::centredLeft, 2);
}

void ProbePanel::resized()
{
    auto b = getLocalBounds().reduced (10, 8);

    header = b.removeFromTop (22);
    b.removeFromTop (6);

    auto buttons = b.removeFromTop (28);
    const int w = buttons.getWidth() / 3;
    selectAll.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    selectNone.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    selectInUse.setBounds (buttons.reduced (2, 0));

    b.removeFromTop (4);
    hint = b.removeFromTop (32);
    b.removeFromTop (4);

    // the map takes the rest of the column; keep cells from getting stretched very tall
    map.setBounds (b.withHeight (jmin (b.getHeight(), 20 + 16 * 40)));
}

//==============================================================================

SequenceBar::SequenceBar (PatternDocument& d) : doc (d)
{
    for (auto* l : { &orderLabel, &stepLabel, &startLabel })
    {
        l->setFont (FontOptions (13.0f));
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }

    orderBox.addItem ("All together", 1);
    orderBox.addItem ("Channel order", 2);
    orderBox.addItem ("Bottom to top (by row)", 3);
    orderBox.addItem ("Top to bottom (by row)", 4);
    orderBox.addItem ("Shank by shank", 5);
    orderBox.setSelectedId (3, dontSendNotification);
    orderBox.setTooltip ("The order the LEDs start in; LEDs with the same rank start together");
    addAndMakeVisible (orderBox);

    stepEditor.setText ("10");
    startEditor.setText ("0");
    stepEditor.setTooltip ("Time between successive LEDs (ranks) in the order");
    startEditor.setTooltip ("When the first LED starts");
    for (auto* t : { &stepEditor, &startEditor })
    {
        t->setInputRestrictions (12, "0123456789.");
        t->setJustification (Justification::centredRight);
        addAndMakeVisible (t);
    }

    addButton.setTooltip ("Add the new-block settings (inspector, with nothing selected) to every selected LED, offset by the step in this order");
    staggerButton.setTooltip ("Re-space the selected blocks by the step in this order, keeping the earliest start");
    addButton.onClick = [this] { addBlocks(); };
    staggerButton.onClick = [this] { staggerBlocks(); };
    addAndMakeVisible (addButton);
    addAndMakeVisible (staggerButton);

    doc.addChangeListener (this);
    refresh();
}

SequenceBar::~SequenceBar()
{
    doc.removeChangeListener (this);
}

void SequenceBar::refresh()
{
    const int leds = (int) doc.selectedLeds().size();
    addButton.setLabel (leds == 0 ? String ("Add blocks (select LEDs)") : "Add blocks to " + String (leds) + (leds == 1 ? " LED" : " LEDs"));
    addButton.setActive (leds > 0);
    staggerButton.setActive (doc.selectedBlocks().size() > 1);
    addButton.repaint();
}

int SequenceBar::controlsWidth() const
{
    // order + step + start
    return 56 + 210 + 84 + 70 + 92 + 70 + 12;
}

int SequenceBar::preferredHeight (int width) const
{
    return (width - 20 >= controlsWidth() + 240 + 170) ? 44 : 78;
}

void SequenceBar::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::widgetBackground));
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void SequenceBar::resized()
{
    auto b = getLocalBounds().reduced (10, 8);
    const bool oneRow = getHeight() < 60;

    auto row = b.removeFromTop (28);
    orderLabel.setBounds (row.removeFromLeft (56));
    orderBox.setBounds (row.removeFromLeft (210).reduced (3, 0));
    stepLabel.setBounds (row.removeFromLeft (84));
    stepEditor.setBounds (row.removeFromLeft (70).reduced (3, 0));
    startLabel.setBounds (row.removeFromLeft (92));
    startEditor.setBounds (row.removeFromLeft (70).reduced (3, 0));

    if (! oneRow)
    {
        b.removeFromTop (6);
        row = b.removeFromTop (28);
    }
    else
    {
        row.removeFromLeft (12);
    }

    staggerButton.setBounds (row.removeFromRight (170).reduced (3, 0));
    addButton.setBounds (row.removeFromRight (jmin (260, row.getWidth())).reduced (3, 0));
}

SequenceOrder SequenceBar::order() const
{
    switch (orderBox.getSelectedId())
    {
        case 1: return SequenceOrder::Together;
        case 2: return SequenceOrder::ChannelOrder;
        case 4: return SequenceOrder::TopToBottom;
        case 5: return SequenceOrder::ShankByShank;
        default: return SequenceOrder::BottomToTop;
    }
}

double SequenceBar::stepSeconds() const
{
    return std::max (0.0, stepEditor.getText().getDoubleValue() / 1000.0);
}

void SequenceBar::addBlocks()
{
    const std::vector<int> leds (doc.selectedLeds().begin(), doc.selectedLeds().end());
    if (leds.empty())
        return;

    const auto ranks = sequenceRanks (leds, order());
    const double step = stepSeconds();
    const double start = std::max (0.0, startEditor.getText().getDoubleValue() / 1000.0);

    std::set<int> added;
    doc.modify ([&] (Pattern& p)
                {
                    int link = 0;
                    if (leds.size() > 1)
                    {
                        for (const auto& b : p.blocks)
                            link = std::max (link, b.link);
                        ++link;
                    }

                    for (std::size_t i = 0; i < leds.size(); ++i)
                    {
                        Block b = doc.newBlockTemplate;
                        b.id = nextBlockId (p);
                        b.led = leds[i];
                        b.start = start + ranks[i] * step;
                        b.link = link;
                        p.blocks.push_back (b);
                        added.insert (b.id);
                    }
                });
    doc.setSelectedBlocks (added);
}

void SequenceBar::staggerBlocks()
{
    const auto sel = doc.selectedBlocks();
    if (sel.size() < 2)
        return;

    // earliest selected block per LED; each LED's blocks shift together
    std::map<int, double> firstStart;
    double base = 1.0e300;
    for (const auto& b : doc.pattern().blocks)
    {
        if (sel.count (b.id) == 0)
            continue;
        auto it = firstStart.find (b.led);
        firstStart[b.led] = (it == firstStart.end()) ? b.start : std::min (it->second, b.start);
        base = std::min (base, b.start);
    }

    std::vector<int> leds;
    for (const auto& kv : firstStart)
        leds.push_back (kv.first);
    const auto ranks = sequenceRanks (leds, order());
    const double step = stepSeconds();

    std::map<int, double> shift;
    for (std::size_t i = 0; i < leds.size(); ++i)
        shift[leds[i]] = base + ranks[i] * step - firstStart[leds[i]];

    doc.modify ([&] (Pattern& p)
                {
                    for (auto& b : p.blocks)
                        if (sel.count (b.id) > 0)
                            b.start = std::max (0.0, b.start + shift[b.led]);
                });
}

} // namespace PatternEditor
