/*
    ------------------------------------------------------------------

    Pattern editor probe map and sequencing tools. See ProbePanel.h.

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
    for (std::size_t i = 0; i < leds.size(); ++i)
    {
        const int led = leds[i];
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
    const float header = 18.0f;
    const float colW = getWidth() / 4.0f;
    const float rowH = (getHeight() - header) / 16.0f;
    const int col = shankOf (led) - 1;
    const int rowFromTop = 15 - rowFromBottom (led);
    return { col * colW + 3.0f, header + rowFromTop * rowH + 1.5f, colW - 6.0f, rowH - 3.0f };
}

int ProbeMap::ledAt (Point<float> p) const
{
    for (int led = 1; led <= kNumLeds; ++led)
        if (cellBounds (led).expanded (1.5f).contains (p))
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

    g.setFont (FontOptions (12.0f));
    for (int s = 0; s < 4; ++s)
    {
        g.setColour (text.withAlpha (0.7f));
        g.drawText ("Shank " + String (s + 1), juce::Rectangle<float> (s * getWidth() / 4.0f, 0, getWidth() / 4.0f, 16.0f), Justification::centred);
    }

    for (int led = 1; led <= kNumLeds; ++led)
    {
        const auto r = cellBounds (led);
        const bool sel = doc.selectedLeds().count (led) > 0;
        const bool inUse = used.count (led) > 0;

        g.setColour (sel ? accent : (inUse ? accent.withAlpha (0.3f) : findColour (ThemeColours::widgetBackground)));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (findColour (ThemeColours::outline));
        g.drawRoundedRectangle (r, 3.0f, 0.75f);

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
        dragBase = doc.selectedLeds();
        return;
    }

    std::set<int> sel = (e.mods.isCommandDown() || e.mods.isShiftDown()) ? doc.selectedLeds() : std::set<int>();

    if (dragToggle && sel.count (led) > 0)
        sel.erase (led);
    else
        sel.insert (led);

    dragBase = sel;
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

    for (auto* b : { &selectAll, &selectNone, &selectInUse, &addButton, &staggerButton })
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

    for (auto* l : { &orderLabel, &stepLabel, &startLabel })
    {
        l->setFont (FontOptions (13.0f));
        addAndMakeVisible (l);
    }

    orderBox.addItem ("All together", 1);
    orderBox.addItem ("Channel order", 2);
    orderBox.addItem ("Bottom to top (by row)", 3);
    orderBox.addItem ("Top to bottom (by row)", 4);
    orderBox.addItem ("Shank by shank", 5);
    orderBox.setSelectedId (3, dontSendNotification);
    addAndMakeVisible (orderBox);

    stepEditor.setText ("10");
    startEditor.setText ("0");
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
}

void ProbePanel::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
    drawSectionHeader (g, *this, ledsHeader, "LEDs");
    drawSectionHeader (g, *this, sequenceHeader, "Sequence");

    g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.65f));
    g.setFont (FontOptions (12.0f));
    g.drawFittedText ("Click, Ctrl-click or drag to select. Tinted LEDs have blocks.", hintArea, Justification::centredLeft, 2);
}

void ProbePanel::resized()
{
    auto b = getLocalBounds().reduced (10, 8);

    // sequence tools at the bottom, fixed height
    auto seq = b.removeFromBottom (22 + 4 * 30 + 2 * 34);
    sequenceHeader = seq.removeFromTop (22);
    seq.removeFromTop (6);
    auto line = [&seq] (int h)
    {
        auto r = seq.removeFromTop (h);
        seq.removeFromTop (4);
        return r;
    };

    auto r = line (26);
    orderLabel.setBounds (r.removeFromLeft (80));
    orderBox.setBounds (r);
    r = line (26);
    stepLabel.setBounds (r.removeFromLeft (80));
    stepEditor.setBounds (r.removeFromLeft (80));
    r = line (26);
    startLabel.setBounds (r.removeFromLeft (80));
    startEditor.setBounds (r.removeFromLeft (80));
    addButton.setBounds (line (30));
    staggerButton.setBounds (line (30));

    b.removeFromBottom (10);

    // LEDs: header, selection buttons, hint, map
    ledsHeader = b.removeFromTop (22);
    b.removeFromTop (6);
    auto buttons = b.removeFromTop (28);
    const int w = buttons.getWidth() / 3;
    selectAll.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    selectNone.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    selectInUse.setBounds (buttons.reduced (2, 0));
    b.removeFromTop (4);
    hintArea = b.removeFromTop (30);

    // keep cells from getting stretched tall on big screens
    const int maxHeight = 18 + 16 * 30;
    map.setBounds (b.withHeight (jmin (b.getHeight(), maxHeight)));
}

SequenceOrder ProbePanel::order() const
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

double ProbePanel::stepSeconds() const
{
    return std::max (0.0, stepEditor.getText().getDoubleValue() / 1000.0);
}

void ProbePanel::addBlocks()
{
    const std::vector<int> leds (doc.selectedLeds().begin(), doc.selectedLeds().end());
    if (leds.empty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Add blocks", "Select one or more LEDs on the probe map first.");
        return;
    }

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

void ProbePanel::staggerBlocks()
{
    const auto sel = doc.selectedBlocks();
    if (sel.empty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Stagger", "Select the blocks to stagger on the timeline first.");
        return;
    }

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
