/*
    ------------------------------------------------------------------

    Pattern editor probe map and sequencing bar. See ProbePanel.h.

    ------------------------------------------------------------------
*/

#include "ProbePanel.h"
#include "WaveformCatalog.h"

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

SequenceOrder orderForTiming (int timingId)
{
    switch (timingId)
    {
        case 2: return SequenceOrder::BottomToTop;
        case 3: return SequenceOrder::TopToBottom;
        case 4: return SequenceOrder::ChannelOrder;
        case 5: return SequenceOrder::ShankByShank;
        default: return SequenceOrder::Together;
    }
}

AddBar::AddBar (PatternDocument& d) : doc (d)
{
    for (auto* l : { &addLabel, &everyLabel, &msLabel })
    {
        l->setFont (FontOptions (13.0f));
        addAndMakeVisible (l);
    }
    addLabel.setFont (FontOptions (13.0f, Font::bold));

    addWaveChoices (waveBox, true);
    waveBox.setSelectedId (1, dontSendNotification); // square pulses
    waveBox.setTooltip ("What to add: a pulse shape, a preset, a classic waveform, or a custom shape. Set amplitude, width, count and rate afterwards in the panel below.");
    waveBox.onChange = [this] { chooseWave(); };
    addAndMakeVisible (waveBox);

    addTimingChoices (timingBox);
    timingBox.setSelectedId (2, dontSendNotification);
    timingBox.setTooltip ("Start all selected LEDs together, or one after another in this order");
    timingBox.onChange = [this] { refresh(); };
    addAndMakeVisible (timingBox);

    stepEditor.setText ("10");
    stepEditor.setInputRestrictions (12, "0123456789.");
    stepEditor.setJustification (Justification::centredRight);
    stepEditor.setTooltip ("Time between LEDs (LEDs in the same row or shank start together)");
    addAndMakeVisible (stepEditor);

    addButton.onClick = [this] { addBlocks(); };
    addAndMakeVisible (addButton);

    chooseWave();
    doc.addChangeListener (this);
    refresh();
}

AddBar::~AddBar()
{
    doc.removeChangeListener (this);
}

void AddBar::chooseWave()
{
    // what "Add" and double-clicking a lane will place
    const int id = waveBox.getSelectedId();
    if (id <= 0)
        return;
    const WaveChoice& c = waveChoices()[(std::size_t) id - 1];

    Block& t = doc.newBlockTemplate;
    t.wave = c.wave;
    t.label = c.label;
    t.fadeIn = t.fadeOut = 0;
    t.length = doc.pattern().duration;
    updateFixedLength (t);
}

void AddBar::refresh()
{
    const int leds = (int) doc.selectedLeds().size();
    addButton.setLabel (leds == 0 ? String ("Select LEDs to add") : "Add to " + String (leds) + (leds == 1 ? " LED" : " LEDs"));
    addButton.setActive (leds > 0);
    addButton.repaint();

    const bool staggered = timingIsStaggered (timingBox.getSelectedId()) && leds != 1;
    everyLabel.setVisible (staggered);
    stepEditor.setVisible (staggered);
    msLabel.setVisible (staggered);
}

int AddBar::preferredHeight (int width) const
{
    return width - 20 >= 40 + 330 + 240 + 50 + 70 + 30 + 220 ? 44 : 78;
}

void AddBar::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::widgetBackground));
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void AddBar::resized()
{
    auto b = getLocalBounds().reduced (10, 8);
    const bool oneRow = getHeight() < 60;

    auto row = b.removeFromTop (28);
    addLabel.setBounds (row.removeFromLeft (40));
    waveBox.setBounds (row.removeFromLeft (jmin (330, row.getWidth())).reduced (3, 0));
    row.removeFromLeft (8);
    timingBox.setBounds (row.removeFromLeft (jmin (240, row.getWidth())).reduced (3, 0));
    everyLabel.setBounds (row.removeFromLeft (50));
    stepEditor.setBounds (row.removeFromLeft (70).reduced (3, 0));
    msLabel.setBounds (row.removeFromLeft (30));

    if (! oneRow)
    {
        b.removeFromTop (6);
        row = b.removeFromTop (28);
    }

    addButton.setBounds (row.removeFromRight (jmin (220, row.getWidth())).reduced (3, 0));
}

void AddBar::addBlocks()
{
    if (doc.selectedLeds().empty())
        return;

    Block prototype = doc.newBlockTemplate;

    if (prototype.wave.kind == WaveKind::Samples && prototype.wave.samples.empty())
    {
        chooseSamplesFile (chooser, [this, prototype] (std::vector<double> values, std::string name) mutable
                           {
                               prototype.wave.samples = std::move (values);
                               prototype.label = name;
                               updateFixedLength (prototype);
                               addBlocksWith (prototype);
                           });
        return;
    }

    addBlocksWith (prototype);
}

void AddBar::addBlocksWith (const Block& prototype)
{
    const std::vector<int> leds (doc.selectedLeds().begin(), doc.selectedLeds().end());
    if (leds.empty())
        return;

    const auto ranks = sequenceRanks (leds, orderForTiming (timingBox.getSelectedId()));
    const double step = std::max (0.0, stepEditor.getText().getDoubleValue() / 1000.0);

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
                        Block b = prototype;
                        b.id = nextBlockId (p);
                        b.led = leds[i];
                        b.start = ranks[i] * step;
                        b.link = link;
                        if (! hasFixedLength (b.wave))
                            b.length = std::max (1.0 / kSampleRateHz, p.duration - b.start);
                        p.blocks.push_back (b);
                        added.insert (b.id);
                    }
                });

    // select what was added so the panel below edits it
    doc.setSelectedBlocks (added);
}

} // namespace PatternEditor
