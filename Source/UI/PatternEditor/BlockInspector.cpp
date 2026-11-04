/*
    ------------------------------------------------------------------

    Pattern editor inspector. See BlockInspector.h.

    ------------------------------------------------------------------
*/

#include "BlockInspector.h"
#include "ProbePanel.h"
#include "WaveformCatalog.h"

#include "../../PatternGen/Formula.h"

#include <cmath>
#include <map>

namespace PatternEditor
{

namespace
{

/** A built-in parameter as shown: label (empty = unused) and the factor
    from the stored value to the shown one (1000: s -> ms). */
struct ParamDesc
{
    const char* label;
    double scale;
};

struct TypeParams
{
    WaveType type;
    ParamDesc params[5]; // freq, amp, offset, duty, step
};

// meanings from paramGuideData in PatternGenGUI.m
const TypeParams kParams[] = {
    { WaveType::None, { { "", 1 }, { "", 1 }, { "", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::Pulse, { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "Duty (%)", 1 }, { "", 1 } } },
    { WaveType::FixedPulse, { { "Pulses", 1 }, { "Amplitude (uA)", 1 }, { "Period (ms)", 1000 }, { "Duty (%)", 1 }, { "", 1 } } },
    { WaveType::PulseStaircase, { { "Pulses", 1 }, { "Start (uA)", 1 }, { "Period (ms)", 1000 }, { "Duty (%)", 1 }, { "Step (uA)", 1 } } },
    { WaveType::Sawtooth, { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::FixedSawtooth, { { "Ramps", 1 }, { "Peak (uA)", 1 }, { "Period (ms)", 1000 }, { "Ramp (%)", 1 }, { "", 1 } } },
    { WaveType::Sine, { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::FixedSine, { { "Cycles", 1 }, { "Amplitude (uA)", 1 }, { "Period (ms)", 1000 }, { "Active (%)", 1 }, { "", 1 } } },
    { WaveType::DoubleSine, { { "Freq 1 (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Freq 2 (Hz)", 1 }, { "Baseline (uA)", 1 }, { "", 1 } } },
    { WaveType::Wavelet, { { "Peaks", 1 }, { "Amplitude (uA)", 1 }, { "Duration (ms)", 1000 }, { "", 1 }, { "", 1 } } },
};

const ParamDesc& paramDesc (WaveType t, int i)
{
    for (const auto& p : kParams)
        if (p.type == t)
            return p.params[i];
    return kParams[0].params[i];
}

double& param (ChannelWaveform& w, int i)
{
    switch (i)
    {
        case 0: return w.freq;
        case 1: return w.amp;
        case 2: return w.offset;
        case 3: return w.duty;
        default: return w.step;
    }
}

String formatNumber (double v)
{
    if (std::isnan (v))
        return "NaN";
    String s (v, 6);
    if (s.containsChar ('.'))
        s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return s;
}

const String kFormulaHelp = "t = seconds since the block starts, T = block length.  e.g. 50*(1+sin(2*pi*8*t)),  mod(),  square(x, duty),  if(cond, a, b)";

} // namespace

BlockInspector::BlockInspector (PatternDocument& d) : doc (d)
{
    title.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (title);

    unlinkButton.setTooltip ("Make the selected blocks independent so each can be edited on its own");
    unlinkButton.onClick = [this]
    {
        const auto sel = doc.selectedBlocks();
        doc.modify ([&] (Pattern& p)
                    {
                        for (auto& b : p.blocks)
                            if (sel.count (b.id) > 0)
                                b.link = 0;
                    });
    };
    deleteButton.onClick = [this]
    {
        const auto sel = doc.selectedBlocks();
        doc.modify ([&] (Pattern& p)
                    { p.blocks.erase (std::remove_if (p.blocks.begin(), p.blocks.end(), [&] (const Block& b)
                                                      { return sel.count (b.id) > 0; }),
                                      p.blocks.end()); });
    };
    addAndMakeVisible (unlinkButton);
    addAndMakeVisible (deleteButton);

    addWaveChoices (waveBox, true);
    waveBox.setTooltip ("Change the waveform of the selected blocks (presets replace all its settings)");
    waveBox.onChange = [this]
    {
        if (! updating)
            chooseWave();
    };
    addAndMakeVisible (waveBox);

    auto setupEditor = [this] (TextEditor& ed, std::function<void()> commit)
    {
        ed.setJustification (Justification::centredRight);
        ed.onReturnKey = commit;
        ed.onFocusLost = commit;
        addAndMakeVisible (ed);
    };

    auto setupLabel = [this] (Label& l)
    {
        l.setFont (FontOptions (13.0f));
        l.setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    };

    for (int i = 0; i < 5; ++i)
    {
        setupLabel (paramLabels[(std::size_t) i]);
        setupEditor (paramEditors[(std::size_t) i], [this, i] { commitParam (i); });
    }

    // points
    setupLabel (pointsLabel);
    pointsLabel.setText ("Points (ms:" + kMicroAmps + ")", dontSendNotification);
    pointsEditor.setTooltip ("time_ms:current pairs, e.g. 0:0, 5:80, 10:0");
    pointsEditor.onReturnKey = pointsEditor.onFocusLost = [this] { commitPoints(); };
    addAndMakeVisible (pointsEditor);
    interpBox.addItem ("Step", 1);
    interpBox.addItem ("Linear", 2);
    interpBox.addItem ("Smooth", 3);
    interpBox.onChange = [this]
    {
        if (updating)
            return;
        const Interpolation i = (Interpolation) (interpBox.getSelectedId() - 1);
        apply ([i] (Block& b) { b.wave.interpolation = i; });
    };
    addAndMakeVisible (interpBox);
    repeatToggle.onClick = [this]
    {
        if (updating)
            return;
        const bool r = repeatToggle.getToggleState();
        apply ([r] (Block& b) { b.wave.repeat = r; });
    };
    addAndMakeVisible (repeatToggle);

    // samples
    importButton.onClick = [this]
    {
        chooseSamplesFile (chooser, [this] (std::vector<double> values, std::string name)
                           { apply ([&] (Block& b)
                                    {
                                        b.wave.kind = WaveKind::Samples;
                                        b.wave.samples = values;
                                        b.label = name;
                                    }); });
    };
    addAndMakeVisible (importButton);
    samplesInfo.setFont (FontOptions (13.0f));
    addAndMakeVisible (samplesInfo);

    // formula
    formulaEditor.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 14.0f, Font::plain));
    formulaEditor.onReturnKey = formulaEditor.onFocusLost = [this] { commitFormula(); };
    formulaEditor.onTextChange = [this]
    {
        std::string error;
        const bool ok = Formula::compile (formulaEditor.getText().toStdString(), error) != nullptr;
        help.setColour (Label::textColourId, ok ? findColour (ThemeColours::defaultText) : Colours::red);
        help.setText (ok ? "Press Enter to apply.   " + kFormulaHelp : String (error), dontSendNotification);
    };
    addAndMakeVisible (formulaEditor);

    // timing
    setupLabel (startLabel);
    setupEditor (startEditor, [this] { commitStart(); });
    setupLabel (lengthLabel);
    setupEditor (lengthEditor, [this] { commitLength(); });
    lengthInfo.setFont (FontOptions (13.0f));
    addAndMakeVisible (lengthInfo);

    setupLabel (spacingLabel);
    setupLabel (everyLabel);
    msLabel.setFont (FontOptions (13.0f));
    addAndMakeVisible (msLabel);
    addTimingChoices (spacingBox);
    spacingBox.setSelectedId (2, dontSendNotification);
    spacingBox.setTooltip ("Line the selected blocks up again: all at once, or one LED after another in this order");
    spacingBox.onChange = [this] { updateVisibility(); };
    addAndMakeVisible (spacingBox);
    spacingStep.setText ("10");
    spacingStep.setInputRestrictions (12, "0123456789.");
    spacingStep.setJustification (Justification::centredRight);
    addAndMakeVisible (spacingStep);
    spacingApply.onClick = [this] { respace(); };
    addAndMakeVisible (spacingApply);

    // more options
    moreButton.setTooltip ("Label, fade in and fade out");
    moreButton.onClick = [this]
    {
        showMore = ! showMore;
        refresh();
    };
    addAndMakeVisible (moreButton);
    setupLabel (labelCaption);
    labelEditor.setTextToShowWhenEmpty ("shown on the blocks", Colours::grey);
    labelEditor.onReturnKey = labelEditor.onFocusLost = [this]
    {
        if (updating)
            return;
        const std::string text = labelEditor.getText().toStdString();
        if (first() != nullptr && text != first()->label)
            apply ([text] (Block& b) { b.label = text; });
    };
    addAndMakeVisible (labelEditor);
    setupLabel (fadeInLabel);
    setupLabel (fadeOutLabel);
    setupEditor (fadeInEditor, [this]
                 {
                     const double v = std::max (0.0, fadeInEditor.getText().getDoubleValue() / 1000.0);
                     if (! updating && first() != nullptr && v != first()->fadeIn)
                         apply ([v] (Block& b) { b.fadeIn = v; });
                 });
    setupEditor (fadeOutEditor, [this]
                 {
                     const double v = std::max (0.0, fadeOutEditor.getText().getDoubleValue() / 1000.0);
                     if (! updating && first() != nullptr && v != first()->fadeOut)
                         apply ([v] (Block& b) { b.fadeOut = v; });
                 });

    help.setFont (FontOptions (13.0f));
    addAndMakeVisible (help);

    doc.addChangeListener (this);
    refresh();
}

BlockInspector::~BlockInspector()
{
    doc.removeChangeListener (this);
}

const Block* BlockInspector::first() const
{
    // the earliest selected block (its start is what the Start field shows)
    const Block* best = nullptr;
    for (const auto& b : doc.pattern().blocks)
        if (doc.isBlockSelected (b.id) && (best == nullptr || b.start < best->start))
            best = &b;
    return best;
}

bool BlockInspector::spansSeveralLeds() const
{
    std::set<int> leds;
    for (const auto& b : doc.pattern().blocks)
        if (doc.isBlockSelected (b.id))
            leds.insert (b.led);
    return leds.size() > 1;
}

void BlockInspector::apply (const std::function<void (Block&)>& change)
{
    const auto sel = doc.selectedBlocks();
    if (sel.empty())
        return;

    doc.modify ([&] (Pattern& p)
                {
                    for (auto& b : p.blocks)
                    {
                        if (sel.count (b.id) == 0)
                            continue;
                        change (b);
                        updateFixedLength (b);
                    }
                });
}

void BlockInspector::setEditorText (TextEditor& ed, const String& text)
{
    if (! ed.hasKeyboardFocus (true) && ed.getText() != text)
        ed.setText (text, false);
}

void BlockInspector::refresh()
{
    const ScopedValueSetter<bool> guard (updating, true);
    const Block* b = first();

    if (b != nullptr)
    {
        const auto n = doc.selectedBlocks().size();
        bool allLinked = b->link != 0;
        for (const auto& x : doc.pattern().blocks)
            if (doc.isBlockSelected (x.id) && x.link != b->link)
                allLinked = false;

        if (n == 1)
            title.setText ("Block on CH " + String (b->led), dontSendNotification);
        else
            title.setText (String ((int) n) + (allLinked ? " linked blocks" : " blocks") + ", edits apply to all", dontSendNotification);

        waveBox.setSelectedId (choiceIdFor (b->wave), dontSendNotification);

        for (int i = 0; i < 5; ++i)
        {
            const auto& pd = paramDesc (b->wave.builtin.type, i);
            paramLabels[(std::size_t) i].setText (String (pd.label).replace ("uA", kMicroAmps), dontSendNotification);
            setEditorText (paramEditors[(std::size_t) i], formatNumber (param (const_cast<ChannelWaveform&> (b->wave.builtin), i) * pd.scale));
        }

        String pts;
        for (const auto& p : b->wave.points)
            pts << (pts.isEmpty() ? "" : ", ") << formatNumber (p.time * 1000.0) << ":" << formatNumber (p.value);
        setEditorText (pointsEditor, pts);
        interpBox.setSelectedId ((int) b->wave.interpolation + 1, dontSendNotification);
        repeatToggle.setToggleState (b->wave.repeat, dontSendNotification);

        samplesInfo.setText (b->wave.samples.empty() ? "No samples yet: import a CSV of " + kMicroAmps + " values at 20 kHz."
                                                     : String ((int) b->wave.samples.size()) + " samples",
                             dontSendNotification);
        setEditorText (formulaEditor, b->wave.formula);

        setEditorText (startEditor, formatNumber (b->start * 1000.0));
        setEditorText (lengthEditor, formatNumber (b->length * 1000.0));
        lengthInfo.setText (lengthDescription (*b), dontSendNotification);

        setEditorText (labelEditor, String::fromUTF8 (b->label.c_str()));
        setEditorText (fadeInEditor, formatNumber (b->fadeIn * 1000.0));
        setEditorText (fadeOutEditor, formatNumber (b->fadeOut * 1000.0));

        moreButton.setLabel (showMore ? "Fewer options" : "More options");
        moreButton.repaint();
        unlinkButton.setActive (allLinked || [&] {
            for (const auto& x : doc.pattern().blocks)
                if (doc.isBlockSelected (x.id) && x.link != 0)
                    return true;
            return false;
        }());

        if (! formulaEditor.hasKeyboardFocus (true))
        {
            help.setColour (Label::textColourId, findColour (ThemeColours::defaultText).withAlpha (0.8f));
            if (b->wave.kind == WaveKind::Builtin)
                help.setText (typeGuide (b->wave.builtin.type), dontSendNotification);
            else if (b->wave.kind == WaveKind::Breakpoints)
                help.setText ("Points are time (ms from the block start) : current (" + kMicroAmps + "). Repeat tiles the shape every last-point time.", dontSendNotification);
            else if (b->wave.kind == WaveKind::Formula)
                help.setText (kFormulaHelp, dontSendNotification);
            else
                help.setText ("Values are " + kMicroAmps + " at 20 kHz (one per 50 " + String::fromUTF8 ("\xc2\xb5") + "s), from the block start.", dontSendNotification);
        }
    }
    else
    {
        title.setText ("No block selected", dontSendNotification);
    }

    updateVisibility();
}

void BlockInspector::updateVisibility()
{
    const Block* b = first();
    const bool sel = b != nullptr;
    const WaveKind k = sel ? b->wave.kind : WaveKind::Builtin;
    const bool fixed = sel && hasFixedLength (b->wave);

    unlinkButton.setVisible (sel);
    deleteButton.setVisible (sel);
    waveBox.setVisible (sel);

    for (int i = 0; i < 5; ++i)
    {
        const bool used = sel && k == WaveKind::Builtin && paramDesc (b->wave.builtin.type, i).label[0] != 0;
        paramLabels[(std::size_t) i].setVisible (used);
        paramEditors[(std::size_t) i].setVisible (used);
    }

    pointsLabel.setVisible (sel && k == WaveKind::Breakpoints);
    pointsEditor.setVisible (sel && k == WaveKind::Breakpoints);
    interpBox.setVisible (sel && k == WaveKind::Breakpoints);
    repeatToggle.setVisible (sel && (k == WaveKind::Breakpoints || k == WaveKind::Samples));
    importButton.setVisible (sel && k == WaveKind::Samples);
    samplesInfo.setVisible (sel && k == WaveKind::Samples);
    formulaEditor.setVisible (sel && k == WaveKind::Formula);

    startLabel.setVisible (sel);
    startEditor.setVisible (sel);
    lengthLabel.setVisible (sel && ! fixed);
    lengthEditor.setVisible (sel && ! fixed);
    lengthInfo.setVisible (sel && fixed);

    const bool spacing = sel && spansSeveralLeds();
    for (Component* c : { (Component*) &spacingLabel, (Component*) &spacingBox, (Component*) &spacingApply })
        c->setVisible (spacing);
    const bool step = spacing && timingIsStaggered (spacingBox.getSelectedId());
    everyLabel.setVisible (step);
    spacingStep.setVisible (step);
    msLabel.setVisible (step);

    moreButton.setVisible (sel);
    for (Component* c : { (Component*) &labelCaption, (Component*) &labelEditor, (Component*) &fadeInLabel,
                          (Component*) &fadeInEditor, (Component*) &fadeOutLabel, (Component*) &fadeOutEditor })
        c->setVisible (sel && showMore);

    help.setVisible (sel);

    resized();
    repaint();

    const int h = getWidth() > 0 ? preferredHeight (getWidth()) : 0;
    if (h != lastPreferredHeight)
    {
        lastPreferredHeight = h;
        if (onLayoutChanged)
            onLayoutChanged();
    }
}

void BlockInspector::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (headerBottom, 10.0f, (float) getWidth() - 10.0f);

    if (! hasSelection())
    {
        g.setColour (findColour (ThemeColours::defaultText).withAlpha (0.7f));
        g.setFont (FontOptions (13.0f));
        g.drawText ("Click a block on the timeline to edit it (Shift or Ctrl to select more). New blocks come from the Add bar above.",
                    getLocalBounds().withTrimmedTop (headerBottom + 4).reduced (12, 0), Justification::centredLeft, true);
    }
}

int BlockInspector::preferredHeight (int width)
{
    return doLayout (width, false);
}

void BlockInspector::resized()
{
    doLayout (getWidth(), true);
}

int BlockInspector::doLayout (int width, bool applyBounds)
{
    const int rowH = 26;
    const int gap = 6;
    const int x0 = 10;
    const int x1 = width - 10;
    int y = 6;

    auto place = [applyBounds] (Component& c, juce::Rectangle<int> r)
    {
        if (applyBounds)
            c.setBounds (r);
    };

    // header: what is selected, and block actions
    {
        juce::Rectangle<int> row (x0, y, x1 - x0, rowH);
        place (deleteButton, row.removeFromRight (84).reduced (3, 0));
        place (unlinkButton, row.removeFromRight (84).reduced (3, 0));
        place (title, row);
        y += rowH + 4;
        headerBottom = y;
        y += gap;
    }

    if (! hasSelection())
        return y + 30;

    // items flow left to right, wrapping onto new rows when the panel is narrow
    struct Item
    {
        Label* label;
        Component* field;
        int fieldWidth;
    };

    auto flow = [&] (const std::vector<Item>& items)
    {
        int x = x0;
        for (const auto& it : items)
        {
            if (! it.field->isVisible())
                continue;
            const int labelW = it.label != nullptr ? textWidth (it.label->getText()) + 10 : 0;
            const int w = labelW + it.fieldWidth;
            if (x > x0 && x + w > x1)
            {
                x = x0;
                y += rowH + gap;
            }
            if (it.label != nullptr)
                place (*it.label, { x, y, labelW, rowH });
            place (*it.field, juce::Rectangle<int> (x + labelW, y, jmin (it.fieldWidth, x1 - x - labelW), rowH).reduced (3, 0));
            x += w + 14;
        }
        y += rowH + gap;
    };

    // waveform and its settings
    std::vector<Item> wave = { { nullptr, &waveBox, 250 } };
    for (int i = 0; i < 5; ++i)
        wave.push_back ({ &paramLabels[(std::size_t) i], &paramEditors[(std::size_t) i], 80 });
    wave.push_back ({ &pointsLabel, &pointsEditor, jmax (220, jmin (480, x1 - x0 - 620)) });
    wave.push_back ({ nullptr, &interpBox, 110 });
    wave.push_back ({ nullptr, &repeatToggle, 90 });
    wave.push_back ({ nullptr, &importButton, 140 });
    wave.push_back ({ nullptr, &samplesInfo, textWidth (samplesInfo.getText()) + 12 });
    wave.push_back ({ nullptr, &formulaEditor, jmax (260, x1 - x0 - 280) });
    flow (wave);

    // timing
    flow ({ { &startLabel, &startEditor, 84 },
            { &lengthLabel, &lengthEditor, 84 },
            { nullptr, &lengthInfo, textWidth (lengthInfo.getText()) + 12 },
            { &spacingLabel, &spacingBox, 220 },
            { &everyLabel, &spacingStep, 60 },
            { nullptr, &msLabel, 26 },
            { nullptr, &spacingApply, 70 },
            { nullptr, &moreButton, 120 } });

    if (showMore)
        flow ({ { &labelCaption, &labelEditor, 260 },
                { &fadeInLabel, &fadeInEditor, 70 },
                { &fadeOutLabel, &fadeOutEditor, 70 } });

    place (help, { x0, y - 2, x1 - x0, 20 });
    y += 22;

    return y + 4;
}

void BlockInspector::chooseWave()
{
    const int id = waveBox.getSelectedId();
    if (id <= 0)
        return;
    const WaveChoice& c = waveChoices()[(std::size_t) id - 1];

    if (c.wave.kind == WaveKind::Samples)
    {
        importButton.triggerClick();
        return;
    }

    const bool preset = c.group == WaveChoice::Group::Preset;
    apply ([&] (Block& b)
           {
               b.wave = c.wave;
               b.label = preset ? c.label : std::string();
           });
}

void BlockInspector::commitParam (int index)
{
    const Block* b = first();
    if (updating || b == nullptr)
        return;

    const auto text = paramEditors[(std::size_t) index].getText().trim();
    if (text.isEmpty())
        return;

    const double value = text.getDoubleValue() / paramDesc (b->wave.builtin.type, index).scale;
    if (param (const_cast<ChannelWaveform&> (b->wave.builtin), index) == value)
        return;

    apply ([index, value] (Block& x) { param (x.wave.builtin, index) = value; });
}

void BlockInspector::commitStart()
{
    const Block* b = first();
    if (updating || b == nullptr || startEditor.getText().trim().isEmpty())
        return;

    // move the whole selection together so its spacing is kept
    const double delta = std::max (0.0, startEditor.getText().getDoubleValue() / 1000.0) - b->start;
    if (delta != 0)
        apply ([delta] (Block& x) { x.start = std::max (0.0, x.start + delta); });
}

void BlockInspector::commitLength()
{
    const Block* b = first();
    if (updating || b == nullptr || lengthEditor.getText().trim().isEmpty())
        return;

    const double v = std::max (1.0 / kSampleRateHz, lengthEditor.getText().getDoubleValue() / 1000.0);
    if (v != b->length)
        apply ([v] (Block& x) { x.length = v; });
}

void BlockInspector::commitPoints()
{
    if (updating || first() == nullptr)
        return;

    std::vector<Breakpoint> points;
    StringArray items;
    items.addTokens (pointsEditor.getText().replaceCharacters (";\n", ",,"), ",", "");
    items.removeEmptyStrings();

    for (const auto& item : items)
    {
        const int colon = item.indexOfChar (':');
        if (colon < 0)
        {
            help.setColour (Label::textColourId, Colours::red);
            help.setText ("Each point is time_ms:current, e.g. 0:0, 5:80, 10:0", dontSendNotification);
            return;
        }
        points.push_back ({ item.substring (0, colon).trim().getDoubleValue() / 1000.0, item.substring (colon + 1).trim().getDoubleValue() });
    }

    for (std::size_t i = 1; i < points.size(); ++i)
    {
        if (points[i].time < points[i - 1].time)
        {
            help.setColour (Label::textColourId, Colours::red);
            help.setText ("Point times must increase.", dontSendNotification);
            return;
        }
    }

    apply ([points] (Block& b) { b.wave.points = points; });
}

void BlockInspector::commitFormula()
{
    const Block* b = first();
    if (updating || b == nullptr)
        return;
    const std::string expr = formulaEditor.getText().toStdString();
    if (expr != b->wave.formula)
        apply ([expr] (Block& x) { x.wave.formula = expr; });
}

void BlockInspector::respace()
{
    const auto sel = doc.selectedBlocks();

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
    if (firstStart.size() < 2)
        return;

    std::vector<int> leds;
    for (const auto& kv : firstStart)
        leds.push_back (kv.first);
    const auto ranks = sequenceRanks (leds, orderForTiming (spacingBox.getSelectedId()));
    const double step = std::max (0.0, spacingStep.getText().getDoubleValue() / 1000.0);

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
