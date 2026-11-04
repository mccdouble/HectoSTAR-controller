/*
    ------------------------------------------------------------------

    Pattern editor inspector. See BlockInspector.h.

    ------------------------------------------------------------------
*/

#include "BlockInspector.h"

#include "../../PatternGen/Formula.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

const String kUa = String::fromUTF8 ("\xc2\xb5" "A");

/** A built-in parameter as shown in the inspector: label (empty = unused)
    and the factor from the stored value to the shown one (1000: s -> ms). */
struct ParamDesc
{
    const char* label;
    double scale;
};

struct TypeDesc
{
    WaveType type;
    const char* name;
    const char* guide;
    ParamDesc params[5]; // freq, amp, offset, duty, step
};

// meanings from paramGuideData in PatternGenGUI.m
const TypeDesc kTypes[] = {
    { WaveType::None, "None", "Off (0 uA).", { { "", 1 }, { "", 1 }, { "", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::Pulse, "Square wave", "Continuous square wave: offset + amplitude x (+1 for the duty fraction of each cycle, else -1).",
      { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "Duty (%)", 1 }, { "", 1 } } },
    { WaveType::FixedPulse, "Pulse train", "A fixed number of pulses, then 0. On level = amplitude; on for the duty fraction of each period.",
      { { "Pulses", 1 }, { "Amplitude (uA)", 1 }, { "Period (ms)", 1000 }, { "Duty (%)", 1 }, { "", 1 } } },
    { WaveType::PulseStaircase, "Pulse staircase", "Pulse train whose amplitude changes by the step each pulse.",
      { { "Pulses", 1 }, { "Start amplitude (uA)", 1 }, { "Period (ms)", 1000 }, { "Duty (%)", 1 }, { "Step (uA)", 1 } } },
    { WaveType::Sawtooth, "Sawtooth", "Continuous sawtooth: offset + amplitude x (ramp from -1 to +1 each cycle).",
      { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::FixedSawtooth, "Ramp train", "A fixed number of ramps from 0 to the peak; the ramp takes the duty fraction of each period.",
      { { "Ramps", 1 }, { "Peak (uA)", 1 }, { "Period (ms)", 1000 }, { "Ramp (%)", 1 }, { "", 1 } } },
    { WaveType::Sine, "Sine", "Continuous sine: offset + amplitude x sin(2 pi f t).",
      { { "Frequency (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Offset (uA)", 1 }, { "", 1 }, { "", 1 } } },
    { WaveType::FixedSine, "Sine cycles", "A fixed number of sine cycles, each compressed into the active fraction of its period (0 elsewhere).",
      { { "Cycles", 1 }, { "Amplitude (uA)", 1 }, { "Period (ms)", 1000 }, { "Active (%)", 1 }, { "", 1 } } },
    { WaveType::DoubleSine, "Double sine", "Baseline + amplitude/2 x (sin f1 + sin f2).",
      { { "Frequency 1 (Hz)", 1 }, { "Amplitude (uA)", 1 }, { "Frequency 2 (Hz)", 1 }, { "Baseline (uA)", 1 }, { "", 1 } } },
    { WaveType::Wavelet, "Wavelet burst", "Positive-only burst (window x cos^2) that fits the duration; peaks is rounded up to odd.",
      { { "Peaks (odd)", 1 }, { "Amplitude (uA)", 1 }, { "Duration (ms)", 1000 }, { "", 1 }, { "", 1 } } },
};

const TypeDesc& typeDesc (WaveType t)
{
    for (const auto& d : kTypes)
        if (d.type == t)
            return d;
    return kTypes[0];
}

String withMicro (const char* s)
{
    return String (s).replace ("uA", kUa);
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

double param (const ChannelWaveform& w, int i)
{
    return param (const_cast<ChannelWaveform&> (w), i);
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

std::string uaLabel (const char* text)
{
    return String (text).replace ("uA", kUa).toStdString();
}

struct Preset
{
    const char* name;
    std::function<void (Block&)> setup;
};

Block builtinPreset (WaveType type, double freq, double amp, double offset, double duty, double step = 0)
{
    Block b;
    b.wave.kind = WaveKind::Builtin;
    b.wave.builtin.type = type;
    b.wave.builtin.freq = freq;
    b.wave.builtin.amp = amp;
    b.wave.builtin.offset = offset;
    b.wave.builtin.duty = duty;
    b.wave.builtin.step = step;
    return b;
}

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        { "10 Hz, 5 ms, 50 uA, 1 s", [] (Block& b) { b.wave = builtinPreset (WaveType::FixedPulse, 10, 50, 0.1, 5).wave; } },
        { "20 Hz, 10 ms, 50 uA, 1 s", [] (Block& b) { b.wave = builtinPreset (WaveType::FixedPulse, 20, 50, 0.05, 20).wave; } },
        { "40 Hz, 2 ms, 50 uA, 0.5 s", [] (Block& b) { b.wave = builtinPreset (WaveType::FixedPulse, 20, 50, 0.025, 8).wave; } },
        { "Single 10 ms pulse, 50 uA", [] (Block& b) { b.wave = builtinPreset (WaveType::FixedPulse, 1, 50, 0.01, 100).wave; } },
        { "Staircase 10 to 80 uA, 8 x 50 ms", [] (Block& b) { b.wave = builtinPreset (WaveType::PulseStaircase, 8, 10, 0.1, 50, 10).wave; } },
        { "Sine 8 Hz, 0 to 100 uA", [] (Block& b) { b.wave = builtinPreset (WaveType::Sine, 8, 50, 50, 0).wave; } },
        { "Ramp 0 to 100 uA over 100 ms", [] (Block& b)
          {
              b.wave = WaveSpec();
              b.wave.kind = WaveKind::Breakpoints;
              b.wave.interpolation = Interpolation::Linear;
              b.wave.points = { { 0, 0 }, { 0.1, 100 } };
          } },
        { "Triangle 0-80-0 uA every 20 ms", [] (Block& b)
          {
              b.wave = WaveSpec();
              b.wave.kind = WaveKind::Breakpoints;
              b.wave.interpolation = Interpolation::Linear;
              b.wave.repeat = true;
              b.wave.points = { { 0, 0 }, { 0.01, 80 }, { 0.02, 0 } };
          } },
        { "Theta-gamma: 40 Hz bursts on an 8 Hz sine (formula)", [] (Block& b)
          {
              b.wave = WaveSpec();
              b.wave.kind = WaveKind::Formula;
              b.wave.formula = "40*(1+sin(2*pi*8*t)) * (1+square(2*pi*40*t, 50))/2";
          } },
    };
    return list;
}

} // namespace

BlockInspector::BlockInspector (PatternDocument& d) : doc (d)
{
    title.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (title);

    presetBox.setTextWhenNothingSelected ("Presets...");
    for (std::size_t i = 0; i < presets().size(); ++i)
        presetBox.addItem (withMicro (presets()[i].name), (int) i + 1);
    presetBox.onChange = [this]
    {
        if (! updating && presetBox.getSelectedId() > 0)
            applyPreset (presetBox.getSelectedId() - 1);
        presetBox.setSelectedId (0, dontSendNotification);
    };
    addAndMakeVisible (presetBox);

    kindBox.addItem ("Built-in", 1);
    kindBox.addItem ("Breakpoints", 2);
    kindBox.addItem ("Samples (CSV)", 3);
    kindBox.addItem ("Formula", 4);
    kindBox.onChange = [this]
    {
        if (! updating)
            changeKind ((WaveKind) (kindBox.getSelectedId() - 1));
    };
    addAndMakeVisible (kindBox);

    for (const auto& t : kTypes)
        typeBox.addItem (t.name, (int) t.type + 1);
    typeBox.onChange = [this]
    {
        if (updating)
            return;
        const WaveType t = (WaveType) (typeBox.getSelectedId() - 1);
        apply ([t] (Block& b) { b.wave.builtin.type = t; });
    };
    addAndMakeVisible (typeBox);

    labelCaption.setFont (FontOptions (13.0f));
    labelCaption.setJustificationType (Justification::centredRight);
    addAndMakeVisible (labelCaption);
    labelEditor.setTextToShowWhenEmpty ("shown on the blocks", Colours::grey);
    labelEditor.onReturnKey = labelEditor.onFocusLost = [this]
    {
        if (updating)
            return;
        const std::string text = labelEditor.getText().toStdString();
        apply ([text] (Block& b) { b.label = text; });
    };
    addAndMakeVisible (labelEditor);

    for (auto* l : { &startLabel, &lengthLabel, &fadeInLabel, &fadeOutLabel, &pointsLabel })
    {
        l->setFont (FontOptions (13.0f));
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }
    pointsLabel.setText ("Points (ms:" + kUa + ")", dontSendNotification);

    for (auto* ed : { &startEditor, &lengthEditor, &fadeInEditor, &fadeOutEditor })
    {
        ed->setJustification (Justification::centredRight);
        ed->onReturnKey = ed->onFocusLost = [this, ed] { commitTiming (*ed); };
        addAndMakeVisible (ed);
    }

    fitButton.setTooltip ("Set the length to the waveform's own length (pulses x period, duration, last point, samples)");
    fitButton.onClick = [this]
    {
        apply ([] (Block& b)
               {
                   const double n = naturalLength (b.wave);
                   if (n > 0)
                       b.length = n;
               });
    };
    addAndMakeVisible (fitButton);

    for (int i = 0; i < 5; ++i)
    {
        paramLabels[(std::size_t) i].setFont (FontOptions (13.0f));
        paramLabels[(std::size_t) i].setJustificationType (Justification::centredRight);
        addAndMakeVisible (paramLabels[(std::size_t) i]);
        auto& ed = paramEditors[(std::size_t) i];
        ed.setJustification (Justification::centredRight);
        ed.onReturnKey = ed.onFocusLost = [this, i] { commitParam (i); };
        addAndMakeVisible (ed);
    }

    guide.setFont (FontOptions (13.0f));
    guide.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (guide);

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

    importButton.onClick = [this] { importCsv(); };
    addAndMakeVisible (importButton);
    samplesInfo.setFont (FontOptions (13.0f));
    addAndMakeVisible (samplesInfo);

    formulaEditor.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 14.0f, Font::plain));
    formulaEditor.onReturnKey = formulaEditor.onFocusLost = [this] { commitFormula(); };
    formulaEditor.onTextChange = [this]
    {
        std::string error;
        const bool ok = Formula::compile (formulaEditor.getText().toStdString(), error) != nullptr;
        formulaHelp.setColour (Label::textColourId, ok ? findColour (ThemeColours::defaultText) : Colours::red);
        formulaHelp.setText (ok ? "Press Enter to apply." : String (error), dontSendNotification);
    };
    addAndMakeVisible (formulaEditor);
    formulaHelp.setFont (FontOptions (13.0f));
    addAndMakeVisible (formulaHelp);

    unlinkButton.setTooltip ("Make the selected blocks independent so they can be edited separately");
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

    doc.addChangeListener (this);
    refresh();
}

BlockInspector::~BlockInspector()
{
    doc.removeChangeListener (this);
}

const Block& BlockInspector::shown() const
{
    if (! editingTemplate())
        if (const Block* b = doc.findBlock (*doc.selectedBlocks().begin()))
            return *b;
    return doc.newBlockTemplate;
}

void BlockInspector::apply (const std::function<void (Block&)>& change)
{
    if (editingTemplate())
    {
        change (doc.newBlockTemplate);
        refresh();
        return;
    }

    const auto sel = doc.selectedBlocks();
    doc.modify ([&] (Pattern& p)
                {
                    for (auto& b : p.blocks)
                        if (sel.count (b.id) > 0)
                            change (b);
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
    const Block& b = shown();

    const auto n = doc.selectedBlocks().size();
    if (n == 0)
        title.setText ("New block settings", dontSendNotification);
    else
        title.setText (String ((int) n) + (n == 1 ? " block selected" : " blocks selected"), dontSendNotification);

    kindBox.setSelectedId ((int) b.wave.kind + 1, dontSendNotification);
    typeBox.setSelectedId ((int) b.wave.builtin.type + 1, dontSendNotification);
    setEditorText (labelEditor, String::fromUTF8 (b.label.c_str()));

    setEditorText (startEditor, formatNumber (b.start * 1000.0));
    setEditorText (lengthEditor, formatNumber (b.length * 1000.0));
    setEditorText (fadeInEditor, formatNumber (b.fadeIn * 1000.0));
    setEditorText (fadeOutEditor, formatNumber (b.fadeOut * 1000.0));

    const TypeDesc& td = typeDesc (b.wave.builtin.type);
    for (int i = 0; i < 5; ++i)
    {
        const auto& pd = td.params[i];
        paramLabels[(std::size_t) i].setText (withMicro (pd.label), dontSendNotification);
        setEditorText (paramEditors[(std::size_t) i], formatNumber (param (b.wave.builtin, i) * pd.scale));
    }

    String pts;
    for (const auto& p : b.wave.points)
        pts << (pts.isEmpty() ? "" : ", ") << formatNumber (p.time * 1000.0) << ":" << formatNumber (p.value);
    setEditorText (pointsEditor, pts);
    interpBox.setSelectedId ((int) b.wave.interpolation + 1, dontSendNotification);
    repeatToggle.setToggleState (b.wave.repeat, dontSendNotification);

    samplesInfo.setText (b.wave.samples.empty() ? "No samples yet: import a CSV of " + kUa + " values at 20 kHz."
                                                : String ((int) b.wave.samples.size()) + " samples (" + formatNumber (b.wave.samples.size() / kSampleRateHz * 1000.0) + " ms)",
                         dontSendNotification);

    setEditorText (formulaEditor, b.wave.formula);
    if (! formulaEditor.hasKeyboardFocus (true))
    {
        formulaHelp.setColour (Label::textColourId, findColour (ThemeColours::defaultText));
        formulaHelp.setText ("t = s since block start, T = block length (s). e.g. 50*(1+sin(2*pi*8*t)), mod(), square(x,duty), if(c,a,b)",
                             dontSendNotification);
    }

    if (b.wave.kind == WaveKind::Builtin)
    {
        guide.setColour (Label::textColourId, findColour (ThemeColours::defaultText));
        guide.setText (withMicro (td.guide), dontSendNotification);
    }
    else if (b.wave.kind == WaveKind::Breakpoints)
    {
        guide.setColour (Label::textColourId, findColour (ThemeColours::defaultText));
        guide.setText ("Points are time (ms from the block start) : current (" + kUa + "). With Repeat, the shape tiles every last-point time.",
                       dontSendNotification);
    }

    unlinkButton.setActive (! editingTemplate());
    deleteButton.setActive (! editingTemplate());

    layoutForKind();
}

void BlockInspector::layoutForKind()
{
    const WaveKind k = shown().wave.kind;
    const bool builtin = k == WaveKind::Builtin;

    typeBox.setVisible (builtin);
    for (int i = 0; i < 5; ++i)
    {
        const bool used = builtin && typeDesc (shown().wave.builtin.type).params[i].label[0] != 0;
        paramLabels[(std::size_t) i].setVisible (used);
        paramEditors[(std::size_t) i].setVisible (used);
    }
    guide.setVisible (builtin || k == WaveKind::Breakpoints);

    pointsLabel.setVisible (k == WaveKind::Breakpoints);
    pointsEditor.setVisible (k == WaveKind::Breakpoints);
    interpBox.setVisible (k == WaveKind::Breakpoints);
    repeatToggle.setVisible (k == WaveKind::Breakpoints || k == WaveKind::Samples);
    importButton.setVisible (k == WaveKind::Samples);
    samplesInfo.setVisible (k == WaveKind::Samples);
    formulaEditor.setVisible (k == WaveKind::Formula);
    formulaHelp.setVisible (k == WaveKind::Formula);

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
}

int BlockInspector::preferredHeight (int width)
{
    return doLayout (width, false);
}

void BlockInspector::resized()
{
    doLayout (getWidth(), true);
}

int BlockInspector::doLayout (int width, bool apply)
{
    const int rowH = 26;
    const int gap = 6;
    const int x0 = 10;
    const int x1 = width - 10;
    int y = 6;

    auto place = [apply] (Component& c, juce::Rectangle<int> r)
    {
        if (apply)
            c.setBounds (r);
    };

    // header: what is being edited, its label, block actions
    {
        juce::Rectangle<int> row (x0, y, x1 - x0, rowH);
        place (deleteButton, row.removeFromRight (84).reduced (3, 0));
        place (unlinkButton, row.removeFromRight (84).reduced (3, 0));
        const int titleW = jmin (textWidth (title.getText(), 14.0f) + 20, row.getWidth() / 2);
        place (title, row.removeFromLeft (titleW));
        place (labelCaption, row.removeFromLeft (textWidth (labelCaption.getText()) + 10));
        place (labelEditor, row.withWidth (jmin (row.getWidth(), 360)).reduced (3, 0));
        y += rowH + 4;
        headerBottom = y;
        y += gap;
    }

    // items flow left to right and wrap onto new rows when the panel is narrow
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

    const Block& b = shown();
    const WaveKind k = b.wave.kind;

    std::vector<Item> wave = { { nullptr, &presetBox, 250 }, { nullptr, &kindBox, 160 } };
    if (k == WaveKind::Builtin)
        wave.push_back ({ nullptr, &typeBox, 200 });
    flow (wave);

    flow ({ { &startLabel, &startEditor, 84 },
            { &lengthLabel, &lengthEditor, 84 },
            { nullptr, &fitButton, 56 },
            { &fadeInLabel, &fadeInEditor, 70 },
            { &fadeOutLabel, &fadeOutEditor, 70 } });

    if (k == WaveKind::Builtin)
    {
        std::vector<Item> params;
        for (int i = 0; i < 5; ++i)
            if (paramEditors[(std::size_t) i].isVisible())
                params.push_back ({ &paramLabels[(std::size_t) i], &paramEditors[(std::size_t) i], 84 });
        if (! params.empty())
            flow (params);
    }
    else if (k == WaveKind::Breakpoints)
    {
        flow ({ { &pointsLabel, &pointsEditor, jmax (220, jmin (520, x1 - x0 - 380)) },
                { nullptr, &interpBox, 120 },
                { nullptr, &repeatToggle, 90 } });
    }
    else if (k == WaveKind::Samples)
    {
        flow ({ { nullptr, &importButton, 140 },
                { nullptr, &repeatToggle, 90 },
                { nullptr, &samplesInfo, jmax (200, textWidth (samplesInfo.getText()) + 10) } });
    }
    else
    {
        flow ({ { nullptr, &formulaEditor, x1 - x0 } });
    }

    // explanation / help / errors
    place (guide, { x0, y - 2, x1 - x0, 20 });
    place (formulaHelp, { x0, y - 2, x1 - x0, 20 });
    y += 22;

    return y + 4;
}

void BlockInspector::commitParam (int index)
{
    if (updating)
        return;
    const auto text = paramEditors[(std::size_t) index].getText().trim();
    if (text.isEmpty())
        return;

    const double scale = typeDesc (shown().wave.builtin.type).params[index].scale;
    const double value = text.getDoubleValue() / scale;
    if (param (shown().wave.builtin, index) == value)
        return;

    apply ([index, value] (Block& b) { param (b.wave.builtin, index) = value; });
}

void BlockInspector::commitTiming (TextEditor& ed)
{
    if (updating || ed.getText().trim().isEmpty())
        return;

    const double v = std::max (0.0, ed.getText().getDoubleValue() / 1000.0);
    const Block& b = shown();

    if (&ed == &startEditor && v != b.start)
    {
        // move the whole selection so staggering is kept
        const double delta = v - b.start;
        apply ([delta] (Block& x) { x.start = std::max (0.0, x.start + delta); });
    }
    else if (&ed == &lengthEditor && v != b.length)
    {
        apply ([v] (Block& x) { x.length = std::max (1.0 / kSampleRateHz, v); });
    }
    else if (&ed == &fadeInEditor && v != b.fadeIn)
    {
        apply ([v] (Block& x) { x.fadeIn = v; });
    }
    else if (&ed == &fadeOutEditor && v != b.fadeOut)
    {
        apply ([v] (Block& x) { x.fadeOut = v; });
    }
}

void BlockInspector::commitPoints()
{
    if (updating)
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
            guide.setColour (Label::textColourId, Colours::red);
            guide.setText ("Each point is time_ms:current, e.g. 0:0, 5:80, 10:0", dontSendNotification);
            return;
        }
        points.push_back ({ item.substring (0, colon).trim().getDoubleValue() / 1000.0, item.substring (colon + 1).trim().getDoubleValue() });
    }

    for (std::size_t i = 1; i < points.size(); ++i)
    {
        if (points[i].time < points[i - 1].time)
        {
            guide.setColour (Label::textColourId, Colours::red);
            guide.setText ("Point times must increase.", dontSendNotification);
            return;
        }
    }

    apply ([points] (Block& b) { b.wave.points = points; });
}

void BlockInspector::commitFormula()
{
    if (updating)
        return;
    const std::string expr = formulaEditor.getText().toStdString();
    if (expr == shown().wave.formula)
        return;
    apply ([expr] (Block& b) { b.wave.formula = expr; });
}

void BlockInspector::applyPreset (int index)
{
    const Preset& preset = presets()[(std::size_t) index];
    const std::string label = uaLabel (preset.name);

    apply ([&] (Block& b)
           {
               preset.setup (b);
               b.label = label;
               const double n = naturalLength (b.wave);
               if (n > 0)
                   b.length = n;
           });
}

void BlockInspector::changeKind (WaveKind kind)
{
    apply ([kind] (Block& b)
           {
               b.wave.kind = kind;
               if (kind == WaveKind::Breakpoints && b.wave.points.empty())
               {
                   b.wave.points = { { 0, 0 }, { 0.005, 80 }, { 0.01, 0 } };
                   b.wave.repeat = true;
               }
               if (kind == WaveKind::Formula && b.wave.formula.empty())
                   b.wave.formula = "50*(1+sin(2*pi*8*t))";
               b.label.clear();
           });
}

void BlockInspector::importCsv()
{
    chooser = std::make_unique<FileChooser> ("Import samples (uA at 20 kHz)", File(), "*.csv;*.txt");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const File f = fc.getResult();
                              if (! f.existsAsFile())
                                  return;

                              // every number in the file, in order (headers and other text are skipped)
                              StringArray tokens;
                              tokens.addTokens (f.loadFileAsString(), ",; \t\r\n", "\"");
                              std::vector<double> values;
                              for (const auto& t : tokens)
                                  if (t.containsAnyOf ("0123456789") && t.trim().containsOnly ("0123456789.-+eE"))
                                      values.push_back (t.getDoubleValue());

                              if (values.empty())
                              {
                                  AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Import samples", "No numbers found in " + f.getFileName() + ".");
                                  return;
                              }

                              const std::string name = f.getFileNameWithoutExtension().toStdString();
                              apply ([&] (Block& b)
                                     {
                                         b.wave.kind = WaveKind::Samples;
                                         b.wave.samples = values;
                                         b.label = name;
                                         if (! b.wave.repeat)
                                             b.length = values.size() / kSampleRateHz;
                                     });
                          });
}

} // namespace PatternEditor
