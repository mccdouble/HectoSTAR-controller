/*
    ------------------------------------------------------------------

    Pattern stim waveform designer. See WaveformDesigner.h.

    ------------------------------------------------------------------
*/

#include "WaveformDesigner.h"

#include "../../PatternGen/Formula.h"

#include <cmath>
#include <map>

namespace PatternEditor
{

namespace
{

/** A parameter as shown: label (empty = unused) and the factor from the
    stored value to the shown one (1000: s -> ms). */
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

ParamDesc paramDesc (const WaveSpec& w, int i)
{
    if (w.kind == WaveKind::Shaped)
    {
        const bool cycles = w.shaped.shape == PulseShape::SineCycle;
        switch (i)
        {
            case 0: return { w.shaped.equalCharge ? "Square equiv. (uA)" : "Peak (uA)", 1 };
            case 1: return { cycles ? "Cycle (ms)" : "Pulse width (ms)", 1000 };
            case 2: return { cycles ? "" : "Period (ms)", 1000 };
            case 3: return { cycles ? "Cycles" : "Pulses", 1 };
            default: return { "Step (uA/pulse)", 1 };
        }
    }
    if (w.kind == WaveKind::Builtin)
        for (const auto& p : kParams)
            if (p.type == w.builtin.type)
                return p.params[i];
    return { "", 1 };
}

double* paramPtr (WaveSpec& w, int i)
{
    if (w.kind == WaveKind::Shaped)
    {
        switch (i)
        {
            case 0: return &w.shaped.amp;
            case 1: return &w.shaped.width;
            case 2: return &w.shaped.period;
            case 3: return &w.shaped.count;
            default: return &w.shaped.step;
        }
    }
    switch (i)
    {
        case 0: return &w.builtin.freq;
        case 1: return &w.builtin.amp;
        case 2: return &w.builtin.offset;
        case 3: return &w.builtin.duty;
        default: return &w.builtin.step;
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

int tabFor (const WaveSpec& w)
{
    switch (w.kind)
    {
        case WaveKind::Shaped: return 0;
        case WaveKind::Builtin: return 1;
        default: return 2;
    }
}

std::vector<int> choicesForTab (int tab)
{
    const auto group = tab == 0 ? WaveChoice::Group::Shape : (tab == 1 ? WaveChoice::Group::Type : WaveChoice::Group::Custom);
    std::vector<int> out;
    const auto& list = waveChoices();
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].group == group)
            out.push_back ((int) i);
    return out;
}

const String kFormulaHelp = "t = seconds since the block starts, T = block length. e.g. 50*(1+sin(2*pi*8*t)), mod(), square(x, duty), if(cond, a, b). Press Enter to apply.";

} // namespace

WaveformDesigner::WaveformDesigner (PatternDocument& d) : doc (d)
{
    working = doc.newBlockTemplate;

    title.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (title);

    auto openTab = [this] (int t)
    {
        // show that tab's settings: keep the waveform if it belongs there, else take the tab's first
        if (tabFor (working.wave) != t)
        {
            const auto choices = choicesForTab (t);
            if (! choices.empty())
            {
                working.wave = waveChoices()[(std::size_t) choices.front()].wave;
                working.label.clear();
            }
        }
        setTab (t);
        workingChanged();
    };
    tabShapes.onClick = [openTab] { openTab (0); };
    tabClassic.onClick = [openTab] { openTab (1); };
    tabCustom.onClick = [openTab] { openTab (2); };
    tabShapes.setTooltip ("Pulse trains of one shape: set peak, width, number of pulses and rate");
    tabClassic.setTooltip ("The waveform types of the MATLAB pattern generator");
    tabCustom.setTooltip ("Your own shape: points, a formula, or samples from a CSV file");
    presetsButton.setTooltip ("Ready-made waveforms");
    presetsButton.onClick = [this] { showPresets(); };
    for (auto* b : { &tabShapes, &tabClassic, &tabCustom, &presetsButton })
        addAndMakeVisible (b);

    palette.onChoose = [this] (int idx) { choose (idx); };
    addAndMakeVisible (palette);

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

    chargeToggle.setTooltip ("Scale the peak so each pulse delivers the same charge as a square pulse of the set amplitude "
                             "(the iso-energy comparison of Bansal et al. 2023)");
    chargeToggle.onClick = [this]
    {
        if (updating)
            return;
        working.wave.shaped.equalCharge = chargeToggle.getToggleState();
        workingChanged();
    };
    addAndMakeVisible (chargeToggle);

    setupLabel (pointsLabel);
    pointsLabel.setText ("Points (ms:" + kMicroAmps + ")", dontSendNotification);
    pointsEditor.setTooltip ("time_ms:current pairs, e.g. 0:0, 5:80, 10:0");
    pointsEditor.onReturnKey = pointsEditor.onFocusLost = [this] { commitPoints(); };
    addAndMakeVisible (pointsEditor);
    interpBox.addItem ("Steps", 1);
    interpBox.addItem ("Straight lines", 2);
    interpBox.addItem ("Smooth", 3);
    interpBox.onChange = [this]
    {
        if (updating)
            return;
        working.wave.interpolation = (Interpolation) (interpBox.getSelectedId() - 1);
        workingChanged();
    };
    addAndMakeVisible (interpBox);
    repeatToggle.onClick = [this]
    {
        if (updating)
            return;
        working.wave.repeat = repeatToggle.getToggleState();
        workingChanged();
    };
    addAndMakeVisible (repeatToggle);

    importButton.onClick = [this]
    {
        chooseSamplesFile (chooser, [this] (std::vector<double> values, std::string name)
                           {
                               working.wave.kind = WaveKind::Samples;
                               working.wave.samples = std::move (values);
                               working.label = name;
                               workingChanged();
                           });
    };
    addAndMakeVisible (importButton);
    samplesInfo.setFont (FontOptions (13.0f));
    addAndMakeVisible (samplesInfo);

    formulaEditor.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 14.0f, Font::plain));
    formulaEditor.onReturnKey = formulaEditor.onFocusLost = [this] { commitFormula(); };
    formulaEditor.onTextChange = [this]
    {
        std::string error;
        const bool ok = Formula::compile (formulaEditor.getText().toStdString(), error) != nullptr;
        help.setColour (Label::textColourId, ok ? findColour (ThemeColours::defaultText) : Colours::red);
        help.setText (ok ? kFormulaHelp : String (error), dontSendNotification);
    };
    addAndMakeVisible (formulaEditor);

    setupLabel (lengthLabel);
    setupEditor (lengthEditor, [this] { commitLength(); });
    lengthEditor.setTooltip ("How long this waveform plays (it would otherwise run to the end of the pattern)");

    help.setFont (FontOptions (12.0f));
    help.setJustificationType (Justification::topLeft);
    help.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (help);

    maths.setFont (FontOptions (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain));
    maths.setJustificationType (Justification::topLeft);
    maths.setMinimumHorizontalScale (1.0f);
    maths.setColour (Label::backgroundColourId, Colours::black.withAlpha (0.08f));
    addAndMakeVisible (maths);
    asFormulaButton.setTooltip ("Turn this pulse train into a Custom formula that plays exactly the same samples, so it can be adapted");
    asFormulaButton.onClick = [this] { convertToFormula(); };
    addAndMakeVisible (asFormulaButton);
    paperLink.setTooltip ("Opens the paper this pulse shape is taken from (Table 6): doi.org/10.3390/photonics10050571");
    paperLink.setFont (FontOptions (12.0f), false, Justification::centredLeft);
    addChildComponent (paperLink);

    addAndMakeVisible (preview);
    cycleButton.onClick = [this]
    {
        preview.setWhole (false);
        updateVisibility();
    };
    wholeButton.onClick = [this]
    {
        preview.setWhole (true);
        updateVisibility();
    };
    addAndMakeVisible (cycleButton);
    addAndMakeVisible (wholeButton);
    info.setFont (FontOptions (12.0f));
    info.setJustificationType (Justification::topLeft);
    addAndMakeVisible (info);

    setupLabel (startLabel);
    setupEditor (startEditor, [] {});
    startEditor.setText ("0", false);
    startEditor.setInputRestrictions (12, "0123456789.");
    startEditor.setTooltip ("When the waveform starts, in ms from the start of the pattern");
    addTimingChoices (timingBox);
    timingBox.setSelectedId (1, dontSendNotification);
    timingBox.setTooltip ("With several LEDs selected: start them all together, or one after another in this order");
    timingBox.onChange = [this] { updateVisibility(); };
    addAndMakeVisible (timingBox);
    setupLabel (everyLabel);
    msLabel.setFont (FontOptions (13.0f));
    addAndMakeVisible (msLabel);
    setupEditor (stepEditor, [] {});
    stepEditor.setText ("50", false);
    stepEditor.setInputRestrictions (12, "0123456789.");
    stepEditor.setTooltip ("Time between one LED (or row, or shank) starting and the next");

    applyButton.setTooltip ("Give the selected LEDs this waveform (replaces what they had)");
    appendButton.setTooltip ("Play this waveform after what each selected LED already has");
    removeButton.setTooltip ("Take the selected LEDs out of the pattern");
    applyButton.onClick = [this] { applyToLeds (false); };
    appendButton.onClick = [this] { applyToLeds (true); };
    removeButton.onClick = [this] { removeFromLeds(); };
    for (auto* b : { &applyButton, &appendButton, &removeButton })
        addAndMakeVisible (b);

    setTab (tabFor (working.wave));
    doc.addChangeListener (this);
    workingChanged();
}

WaveformDesigner::~WaveformDesigner()
{
    doc.removeChangeListener (this);
}

std::vector<int> WaveformDesigner::selectedLeds() const
{
    return { doc.selectedLeds().begin(), doc.selectedLeds().end() };
}

void WaveformDesigner::changeListenerCallback (ChangeBroadcaster*)
{
    if (doc.selectedLeds() != loadedFor)
    {
        loadedFor = doc.selectedLeds();
        loadFromSelection();
    }
    updateVisibility();
}

void WaveformDesigner::loadFromSelection()
{
    // the earliest block on the selected LEDs, so it can be adjusted and re-applied
    const Block* found = nullptr;
    double firstStart = 1.0e300;
    for (const auto& b : doc.pattern().blocks)
    {
        if (doc.selectedLeds().count (b.led) == 0)
            continue;
        if (found == nullptr || b.start < found->start)
            found = &b;
        firstStart = jmin (firstStart, b.start);
    }

    if (found == nullptr)
        return; // keep the current waveform for LEDs without one

    working = *found;
    startEditor.setText (formatNumber (firstStart * 1000.0), false);
    setTab (tabFor (working.wave));
    workingChanged();
}

void WaveformDesigner::setTab (int t)
{
    tab = t;
    tabShapes.setStyle (tab == 0 ? EditorButton::Style::Accent : EditorButton::Style::Normal);
    tabClassic.setStyle (tab == 1 ? EditorButton::Style::Accent : EditorButton::Style::Normal);
    tabCustom.setStyle (tab == 2 ? EditorButton::Style::Accent : EditorButton::Style::Normal);
    palette.setChoices (choicesForTab (tab));
    palette.setSelected (choiceIdFor (working.wave) - 1);
    resized();
}

void WaveformDesigner::choose (int idx)
{
    const WaveChoice& c = waveChoices()[(std::size_t) idx];

    if (c.wave.kind == WaveKind::Samples)
    {
        importButton.triggerClick();
        return;
    }

    if (c.wave.kind == WaveKind::Shaped && working.wave.kind == WaveKind::Shaped)
    {
        // switching shape keeps the amplitude, width, period and count
        const bool fromSine = working.wave.shaped.shape == PulseShape::SineCycle;
        working.wave.shaped.shape = c.wave.shaped.shape;
        if (c.wave.shaped.shape == PulseShape::SineCycle && shapedCount (working.wave.shaped) > 1)
            working.wave.shaped.width = working.wave.shaped.period; // one cycle per period: a continuous sine
        else if (fromSine)
            working.wave.shaped = c.wave.shaped; // sine cycles have no gap: start from the shape's defaults
    }
    else
    {
        working.wave = c.wave;
    }
    working.label.clear();
    workingChanged();
}

void WaveformDesigner::showPresets()
{
    PopupMenu menu;
    const auto& list = waveChoices();
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].group == WaveChoice::Group::Preset)
            menu.addItem ((int) i + 1, list[i].name);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetsButton),
                        [this] (int result)
                        {
                            if (result <= 0)
                                return;
                            const auto& c = waveChoices()[(std::size_t) result - 1];
                            working.wave = c.wave;
                            working.label = c.label;
                            setTab (tabFor (working.wave));
                            workingChanged();
                        });
}

void WaveformDesigner::workingChanged()
{
    updateFixedLength (working);
    if (working.length <= 0)
        working.length = doc.pattern().duration;

    doc.newBlockTemplate = working;
    preview.setBlock (working);
    palette.setSelected (choiceIdFor (working.wave) - 1);
    refreshFields();
    updateVisibility();
}

void WaveformDesigner::refreshFields()
{
    const ScopedValueSetter<bool> guard (updating, true);

    WaveSpec spec = working.wave;
    for (int i = 0; i < 5; ++i)
    {
        const auto pd = paramDesc (spec, i);
        paramLabels[(std::size_t) i].setText (String (pd.label).replace ("uA", kMicroAmps), dontSendNotification);
        if (pd.label[0] != 0)
            paramEditors[(std::size_t) i].setText (formatNumber (*paramPtr (spec, i) * pd.scale), false);
    }
    chargeToggle.setToggleState (working.wave.shaped.equalCharge, dontSendNotification);

    String pts;
    for (const auto& p : working.wave.points)
        pts << (pts.isEmpty() ? "" : ", ") << formatNumber (p.time * 1000.0) << ":" << formatNumber (p.value);
    pointsEditor.setText (pts, false);
    interpBox.setSelectedId ((int) working.wave.interpolation + 1, dontSendNotification);
    repeatToggle.setToggleState (working.wave.repeat, dontSendNotification);
    samplesInfo.setText (working.wave.samples.empty() ? "No samples yet" : String ((int) working.wave.samples.size()) + " samples", dontSendNotification);
    if (! formulaEditor.hasKeyboardFocus (true))
        formulaEditor.setText (String::fromUTF8 (working.wave.formula.c_str()), false);
    lengthEditor.setText (formatNumber (working.length * 1000.0), false);

    const String len = lengthDescription (working);
    info.setText (len.isEmpty() ? "Plays for the length you set." : len.substring (0, 1).toUpperCase() + len.substring (1), dontSendNotification);

    help.setColour (Label::textColourId, findColour (ThemeColours::defaultText).withAlpha (0.85f));
    switch (working.wave.kind)
    {
        case WaveKind::Shaped: help.setText (shapeGuide (working.wave.shaped.shape), dontSendNotification); break;
        case WaveKind::Builtin: help.setText (typeGuide (working.wave.builtin.type), dontSendNotification); break;
        case WaveKind::Breakpoints: help.setText ("Points are time (ms) : current (" + kMicroAmps + "). Repeat tiles the shape every last-point time. Press Enter to apply.", dontSendNotification); break;
        case WaveKind::Formula: help.setText (kFormulaHelp, dontSendNotification); break;
        case WaveKind::Samples: help.setText ("Values are " + kMicroAmps + " at 20 kHz (one per 50 " + String::fromUTF8 ("\xc2\xb5") + "s).", dontSendNotification); break;
    }

    updateMaths();
}

void WaveformDesigner::updateVisibility()
{
    const WaveKind k = working.wave.kind;

    for (int i = 0; i < 5; ++i)
    {
        bool used = paramDesc (working.wave, i).label[0] != 0;
        if (used && k == WaveKind::Shaped && i == 2)
            used = shapedCount (working.wave.shaped) > 1; // the period only matters with several pulses
        paramLabels[(std::size_t) i].setVisible (used);
        paramEditors[(std::size_t) i].setVisible (used);
    }
    chargeToggle.setVisible (k == WaveKind::Shaped && working.wave.shaped.shape != PulseShape::Square);
    maths.setVisible (k == WaveKind::Shaped);
    asFormulaButton.setVisible (k == WaveKind::Shaped);
    paperLink.setVisible (k == WaveKind::Shaped && ! pulseShapePaperName (working.wave.shaped.shape).empty());

    pointsLabel.setVisible (k == WaveKind::Breakpoints);
    pointsEditor.setVisible (k == WaveKind::Breakpoints);
    interpBox.setVisible (k == WaveKind::Breakpoints);
    repeatToggle.setVisible (k == WaveKind::Breakpoints || k == WaveKind::Samples);
    importButton.setVisible (k == WaveKind::Samples);
    samplesInfo.setVisible (k == WaveKind::Samples);
    formulaEditor.setVisible (k == WaveKind::Formula);

    const bool fixed = hasFixedLength (working.wave);
    lengthLabel.setVisible (! fixed);
    lengthEditor.setVisible (! fixed);

    cycleButton.setStyle (preview.showsWhole() ? EditorButton::Style::Normal : EditorButton::Style::Accent);
    wholeButton.setStyle (preview.showsWhole() ? EditorButton::Style::Accent : EditorButton::Style::Normal);

    const int n = (int) doc.selectedLeds().size();
    const bool several = n > 1;
    timingBox.setVisible (several);
    const bool staggered = several && timingIsStaggered (timingBox.getSelectedId());
    everyLabel.setVisible (staggered);
    stepEditor.setVisible (staggered);
    msLabel.setVisible (staggered);

    int withBlocks = 0;
    for (int led : doc.selectedLeds())
        for (const auto& b : doc.pattern().blocks)
            if (b.led == led)
            {
                ++withBlocks;
                break;
            }

    const String leds = n == 1 ? "LED " + String (*doc.selectedLeds().begin()) : String (n) + " LEDs";
    title.setText (n == 0 ? String ("Waveform (select LEDs to apply it)") : "Waveform for " + leds, dontSendNotification);
    applyButton.setLabel ("Apply to " + (n == 0 ? String ("LEDs") : leds));
    applyButton.setActive (n > 0);
    appendButton.setActive (withBlocks > 0);
    removeButton.setActive (withBlocks > 0);
    applyButton.repaint();

    resized();
}

void WaveformDesigner::updateMaths()
{
    if (working.wave.kind != WaveKind::Shaped)
    {
        maths.setText ({}, dontSendNotification);
        return;
    }

    const auto& t = working.wave.shaped;
    const String uA = " " + kMicroAmps;
    auto num = [] (double v, int decimals = 3)
    {
        String s (v, decimals);
        if (s.containsChar ('.'))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s;
    };

    const bool cycles = t.shape == PulseShape::SineCycle;
    const int n = shapedCount (t);
    const double periodMs = 1000.0 * shapedPeriodSamples (t) / kSampleRateHz;
    const double widthMs = 1000.0 * shapedPulseSamples (t) / kSampleRateHz;
    const String paper = String (pulseShapePaperName (t.shape));

    String text;
    text << "Shape   f(x) = " << String (pulseShapeMaths (t.shape)) << "\n"
         << "        x goes 0 -> 1 across each " << num (widthMs) << " ms " << (cycles ? "cycle" : "pulse");
    if (paper.isNotEmpty())
        text << "   (paper: \"" << paper << "\")";

    String amp = num (t.amp);
    if (t.step != 0)
        amp = "(" + num (t.amp) + (t.step > 0 ? " + " : " - ") + num (std::abs (t.step)) + " k)";

    text << "\nTrain   " << (cycles ? "cycle" : "pulse") << " k = 0.." << (n - 1) << " starts at k x " << num (periodMs) << " ms;  current = "
         << amp << uA << " x " << (t.equalCharge ? "g x " : "") << "f(x)";

    if (t.equalCharge)
    {
        const double mean = pulseShapeMean (t.shape, shapedPulseSamples (t));
        text << "\nCharge  g = 1 / mean of f = 1 / " << num (mean, 4) << " = " << num (1.0 / mean, 3)
             << "  (peak " << num (shapedMaxPeak (t), 1) << uA << ", same charge as a square pulse)";
    }

    maths.setText (text, dontSendNotification);
}

void WaveformDesigner::convertToFormula()
{
    if (working.wave.kind != WaveKind::Shaped)
        return;

    const ShapedTrain t = working.wave.shaped;
    working.length = naturalLength (working.wave);
    working.wave.formula = shapedTrainFormula (t);
    working.wave.kind = WaveKind::Formula;
    working.label = shapeName (t.shape).toStdString() + " (formula)";
    setTab (2);
    workingChanged();
}

void WaveformDesigner::commitParam (int index)
{
    if (updating)
        return;
    const auto text = paramEditors[(std::size_t) index].getText().trim();
    if (text.isEmpty())
        return;

    const double value = text.getDoubleValue() / paramDesc (working.wave, index).scale;
    double* p = paramPtr (working.wave, index);
    if (*p == value)
        return;

    *p = value;
    workingChanged();
}

void WaveformDesigner::commitPoints()
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

    working.wave.points = points;
    workingChanged();
}

void WaveformDesigner::commitFormula()
{
    if (updating)
        return;
    const std::string expr = formulaEditor.getText().toStdString();
    if (expr == working.wave.formula)
        return;
    working.wave.formula = expr;
    workingChanged();
}

void WaveformDesigner::commitLength()
{
    if (updating || lengthEditor.getText().trim().isEmpty())
        return;
    const double v = jmax (1.0 / kSampleRateHz, lengthEditor.getText().getDoubleValue() / 1000.0);
    if (v == working.length)
        return;
    working.length = v;
    workingChanged();
}

void WaveformDesigner::applyToLeds (bool append)
{
    const auto leds = selectedLeds();
    if (leds.empty())
        return;

    if (working.wave.kind == WaveKind::Samples && working.wave.samples.empty())
    {
        importButton.triggerClick();
        return;
    }

    const bool several = leds.size() > 1;
    const auto ranks = sequenceRanks (leds, several ? orderForTiming (timingBox.getSelectedId()) : SequenceOrder::Together);
    const double step = jmax (0.0, stepEditor.getText().getDoubleValue() / 1000.0);
    const double start = jmax (0.0, startEditor.getText().getDoubleValue() / 1000.0);

    doc.modify ([&] (Pattern& p)
                {
                    // where each LED's existing blocks end (for Add after)
                    std::map<int, double> ends;
                    for (const auto& b : p.blocks)
                        ends[b.led] = jmax (ends[b.led], b.start + b.length);

                    if (! append)
                        p.blocks.erase (std::remove_if (p.blocks.begin(), p.blocks.end(), [&] (const Block& b)
                                                        { return std::find (leds.begin(), leds.end(), b.led) != leds.end(); }),
                                        p.blocks.end());

                    int link = 0;
                    if (several)
                    {
                        for (const auto& b : p.blocks)
                            link = jmax (link, b.link);
                        ++link;
                    }

                    double lastEnd = 0;
                    for (std::size_t i = 0; i < leds.size(); ++i)
                    {
                        Block b = working;
                        b.id = nextBlockId (p);
                        b.led = leds[i];
                        b.link = link;
                        b.fadeIn = working.fadeIn;
                        b.fadeOut = working.fadeOut;

                        if (append && ends.count (leds[i]) > 0)
                            b.start = ends[leds[i]];
                        else
                            b.start = start + ranks[i] * step;

                        updateFixedLength (b);
                        if (! hasFixedLength (b.wave) && b.length <= 0)
                            b.length = jmax (1.0 / kSampleRateHz, p.duration - b.start);

                        lastEnd = jmax (lastEnd, b.start + b.length);
                        p.blocks.push_back (b);
                    }

                    // make room: lengthen the pattern to fit (whole 0.1 s)
                    if (lastEnd > p.duration + 1.0e-9)
                        p.duration = std::ceil (lastEnd * 10.0 - 1.0e-9) / 10.0;
                });
}

void WaveformDesigner::removeFromLeds()
{
    const auto leds = selectedLeds();
    doc.modify ([&] (Pattern& p)
                { p.blocks.erase (std::remove_if (p.blocks.begin(), p.blocks.end(), [&] (const Block& b)
                                                  { return std::find (leds.begin(), leds.end(), b.led) != leds.end(); }),
                                  p.blocks.end()); });
}

void WaveformDesigner::paint (Graphics& g)
{
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (palette.getBottom() + 6, 0.0f, (float) getWidth());
}

void WaveformDesigner::resized()
{
    const int W = getWidth();
    const int rowH = 26;
    const int gap = 6;

    auto top = juce::Rectangle<int> (0, 0, W, rowH);
    presetsButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (16);
    removeButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (4);
    appendButton.setBounds (top.removeFromRight (90));
    top.removeFromRight (4);
    applyButton.setBounds (top.removeFromRight (jmin (170, jmax (110, top.getWidth() - 120))));
    top.removeFromRight (8);
    title.setBounds (top);

    auto tabs = juce::Rectangle<int> (0, rowH + 6, W, rowH);
    tabShapes.setBounds (tabs.removeFromLeft (120));
    tabs.removeFromLeft (4);
    tabClassic.setBounds (tabs.removeFromLeft (90));
    tabs.removeFromLeft (4);
    tabCustom.setBounds (tabs.removeFromLeft (90));

    const int paletteY = tabs.getBottom() + 8;
    palette.setBounds (0, paletteY, W, palette.preferredHeight (W));

    // lower part: settings on the left, preview on the right
    const int lowerY = palette.getBottom() + 14;
    const int previewW = jlimit (240, 380, (int) (W * 0.4));
    const int leftW = W - previewW - 14;
    const int x1 = leftW;

    const int ph = jmax (120, jmin (170, getHeight() - lowerY - 60));
    preview.setBounds (W - previewW, lowerY, previewW, ph);
    auto row = juce::Rectangle<int> (W - previewW, lowerY + ph + 4, previewW, 24);
    cycleButton.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (4);
    wholeButton.setBounds (row.removeFromLeft (70));
    info.setBounds (W - previewW, lowerY + ph + 30, previewW, jmax (20, getHeight() - (lowerY + ph + 30)));

    int y = lowerY;
    struct Item
    {
        Label* label;
        Component* field;
        int fieldWidth;
    };
    auto flow = [&] (const std::vector<Item>& items)
    {
        int x = 0;
        bool any = false;
        for (const auto& it : items)
        {
            if (! it.field->isVisible())
                continue;
            any = true;
            const int labelW = it.label != nullptr ? textWidth (it.label->getText()) + 8 : 0;
            const int w = labelW + it.fieldWidth;
            if (x > 0 && x + w > x1)
            {
                x = 0;
                y += rowH + gap;
            }
            if (it.label != nullptr)
                it.label->setBounds (x, y, labelW, rowH);
            it.field->setBounds (juce::Rectangle<int> (x + labelW, y, jmin (it.fieldWidth, x1 - x - labelW), rowH));
            x += w + 12;
        }
        if (any)
            y += rowH + gap;
    };

    std::vector<Item> params;
    for (int i = 0; i < 5; ++i)
        params.push_back ({ &paramLabels[(std::size_t) i], &paramEditors[(std::size_t) i], 64 });
    params.push_back ({ nullptr, &chargeToggle, 120 });
    params.push_back ({ &pointsLabel, &pointsEditor, jmax (160, x1 - 260) });
    params.push_back ({ nullptr, &interpBox, 130 });
    params.push_back ({ nullptr, &repeatToggle, 80 });
    params.push_back ({ nullptr, &importButton, 120 });
    params.push_back ({ nullptr, &samplesInfo, 120 });
    params.push_back ({ nullptr, &formulaEditor, jmax (200, x1) });
    params.push_back ({ &lengthLabel, &lengthEditor, 70 });
    flow (params);

    help.setBounds (0, y - 2, x1, 34);
    y += 36;

    if (maths.isVisible())
    {
        const int lines = jmax (1, StringArray::fromLines (maths.getText()).size());
        const int h = lines * 15 + 8;
        maths.setBounds (0, y, x1, h);
        y += h + 4;
        asFormulaButton.setBounds (0, y, 130, 24);
        paperLink.setBounds (140, y, jmax (0, x1 - 140), 24);
        y += 30;
    }

    flow ({ { &startLabel, &startEditor, 64 },
            { nullptr, &timingBox, 220 },
            { &everyLabel, &stepEditor, 54 },
            { nullptr, &msLabel, 24 } });
}

} // namespace PatternEditor
