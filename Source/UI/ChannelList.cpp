/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ChannelList.h"
#include "ChannelCanvas.h"
#include "ChannelComponent.h"

#include "../DeviceEditor.h"
#include "../DeviceThread.h"
#include "../Headstage.h"
#include "PatternEditor/PatternPanel.h"
#include "PatternEditor/WaveformViews.h"

using namespace RhythmNode;

/** Small always-on-top window showing pattern upload progress. Not modal, so the rest
    of the GUI (other plugins, LFP viewer, etc.) stays usable while flash is written. */
class ChannelList::PatternUploadWindow : public DocumentWindow,
                                         private Timer
{
public:
    PatternUploadWindow (DeviceThread* board_, const File& file, Component* centreAround)
        : DocumentWindow ("Uploading pattern stim file",
                          LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                          0), // no close/minimise buttons; it closes itself when the upload finishes
          board (board_)
    {
        setContentOwned (new Content (file.getFileName() + " (" + String (file.getSize()) + " bytes)", progress), true);
        setResizable (false, false);
        setAlwaysOnTop (true);
        centreAroundComponent (centreAround, getWidth(), getHeight());
        setVisible (true);
        startTimerHz (10);
    }

private:
    class Content : public Component
    {
    public:
        Content (const String& text, double& progress) : bar (progress)
        {
            label.setText (text, dontSendNotification);
            label.setJustificationType (Justification::centred);
            addAndMakeVisible (label);
            addAndMakeVisible (bar);
            setSize (340, 80);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            label.setBounds (area.removeFromTop (24));
            area.removeFromTop (8);
            bar.setBounds (area.removeFromTop (24));
        }

    private:
        Label label;
        ProgressBar bar;
    };

    void timerCallback() override
    {
        progress = board->getPatternUploadProgress();
    }

    DeviceThread* board;
    double progress = 0.0; // read by the ProgressBar on the message thread
};

namespace
{

// probe drawing geometry (paint() and the LED positions share it)
constexpr float kProbeX = 55.0f;  // left of the first shank
constexpr float kProbeY = 82.0f;  // top of the shanks
constexpr float kShankW = 90.0f;
constexpr float kShankH = 500.0f;
constexpr float kShankGap = 16.0f;
constexpr int kProbeColumnW = 495; // the settings panel starts right of this
constexpr int kTopBarH = 46;
constexpr int kSelectRowY = (int) (kProbeY + kShankH + 12.0f);

Colour ledColour() { return Colour (0xff4169e1); } // royal blue: has a waveform / will pulse
Colour playColour() { return Colour (0xff4fc3ff); } // light blue: an LED lit during playback

/** "1-4, 9, 17-20" for a set of LED numbers. */
String formatLedRange (const std::set<int>& leds)
{
    String out;
    auto it = leds.begin();
    while (it != leds.end())
    {
        const int first = *it;
        int last = first;
        auto next = std::next (it);
        while (next != leds.end() && *next == last + 1)
        {
            last = *next;
            ++next;
        }
        if (out.isNotEmpty())
            out << ", ";
        out << first;
        if (last > first)
            out << "-" << last;
        it = next;
    }
    return out;
}

/** Parses "1-4, 9, 17-20" (as PatternGenGUI.m's parseRangeString). Returns
    false if anything is not a number, a range a-b with a <= b, or 1..maxLed. */
bool parseLedRange (const String& text, int maxLed, std::set<int>& leds)
{
    leds.clear();
    StringArray parts;
    parts.addTokens (text, ",;", "");
    for (auto part : parts)
    {
        part = part.trim();
        if (part.isEmpty())
            continue;
        const int dash = part.indexOfChar ('-');
        const String a = dash < 0 ? part : part.substring (0, dash).trim();
        const String b = dash < 0 ? part : part.substring (dash + 1).trim();
        if (! a.containsOnly ("0123456789") || ! b.containsOnly ("0123456789") || a.isEmpty() || b.isEmpty())
            return false;
        const int lo = a.getIntValue(), hi = b.getIntValue();
        if (lo < 1 || hi > maxLed || lo > hi)
            return false;
        for (int i = lo; i <= hi; ++i)
            leds.insert (i);
    }
    return true;
}

} // namespace

/** The LEDs on the probe drawing, the same in both modes: numbered buttons,
    click to toggle. Selected LEDs are highlighted; in pattern mode LEDs
    with a waveform are blue, in pulse mode the selected (ticked) LEDs are
    the ones that pulse. */
class ChannelList::ProbeLedOverlay : public Component,
                                     public TooltipClient
{
public:
    explicit ProbeLedOverlay (ChannelList& owner_) : owner (owner_) {}

    void paint (Graphics& g) override
    {
        const auto sel = owner.ledSelection();
        const bool pattern = owner.selectedStimOutputType != 0;
        const Colour accent = findColour (ThemeColours::highlightedFill);
        const auto& playback = owner.patternPanel->getPlayback();
        const bool playing = pattern && playback.isActive();

        for (int i = 0; i < owner.visibleLeds(); ++i)
        {
            const int led = i + 1;
            const auto c = owner.ledCentre (i);
            const juce::Rectangle<float> r (c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f);
            const bool isSel = sel.count (led) > 0;
            const bool blue = pattern ? owner.ledHasWaveform (led) : isSel;

            if (pattern && isSel)
            {
                g.setColour (accent);
                g.fillEllipse (r.expanded (3.5f));
            }

            if (playing)
            {
                // light output: glow and brightness follow the current (0..100 uA)
                const float lit = jlimit (0.0f, 1.0f, playback.level (led) / 100.0f);
                if (lit > 0)
                {
                    g.setColour (playColour().withAlpha (0.35f * lit + 0.1f));
                    g.fillEllipse (r.expanded (4.0f + 8.0f * lit));
                }
                g.setColour (Colour (0xff20242a).interpolatedWith (playColour(), lit > 0 ? 0.35f + 0.65f * lit : 0.0f));
                g.fillEllipse (r);
                g.setColour (Colours::black.withAlpha (0.8f));
                g.drawEllipse (r, 1.2f);
                g.setColour (lit > 0.5f ? Colours::black : Colours::white.withAlpha (0.8f));
                g.setFont (FontOptions (10.0f, Font::bold));
                g.drawText (String (led), r.toNearestInt(), Justification::centred);
                continue;
            }

            g.setColour (blue ? ledColour() : Colours::white.withAlpha (isEnabled() ? 0.45f : 0.2f));
            g.fillEllipse (r);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.drawEllipse (r, 1.2f);

            g.setColour (blue ? Colours::white : Colours::black);
            g.setFont (FontOptions (10.0f, Font::bold));
            g.drawText (String (led), r.toNearestInt(), Justification::centred);
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        const int led = ledAt (e.position);
        if (led == 0 || ! isEnabled())
            return;

        auto sel = owner.ledSelection();
        if (sel.count (led) > 0)
            sel.erase (led);
        else
            sel.insert (led);
        owner.setLedSelection (sel);
    }

    void mouseMove (const MouseEvent& e) override { hovered = ledAt (e.position); }

    String getTooltip() override
    {
        if (hovered == 0)
            return {};
        const int row = PatternEditor::rowFromBottom (hovered) + 1;
        return "LED " + String (hovered) + " (shank " + String (PatternEditor::shankOf (hovered)) + ", " + String (row)
               + (row == 1 ? "st" : row == 2 ? "nd" : row == 3 ? "rd" : "th") + " from the tip). Click to select or deselect.";
    }

    bool hitTest (int x, int y) override { return ledAt ({ (float) x, (float) y }) != 0; }

private:
    ChannelList& owner;
    int hovered = 0;

    int ledAt (Point<float> p) const
    {
        for (int i = 0; i < owner.visibleLeds(); ++i)
            if (owner.ledCentre (i).getDistanceFrom (p) <= 12.0f)
                return i + 1;
        return 0;
    }
};

ChannelList::~ChannelList()
{
    if (patternPanel != nullptr)
        patternPanel->getPlayback().removeChangeListener (this);
    if (patternDoc != nullptr)
        patternDoc->removeChangeListener (this);
}

ChannelList::ChannelList (DeviceThread* board_, DeviceEditor* editor_) : board (board_), editor (editor_), maxChannels (0)
{
    channelComponents.clear();

    //Probe Geometry Selection
    probeTypeSelect = std::make_unique<ComboBox> ("Probe Select");
    probeTypeSelect->addItem ("4-Shank Chronic", 1);
    probeTypeSelect->addItem ("2-Shank Chronic", 2);
    probeTypeSelect->addItem ("4-Shank Acute", 3);
    probeTypeSelect->addItem ("Demo Board", 4);
    probeTypeSelect->setTooltip ("Probe on the headstage: sets the drawing and the LED channel mapping");
    probeTypeSelect->addListener (this);
    probeTypeSelect->setSelectedId (1, dontSendNotification);
    addAndMakeVisible (probeTypeSelect.get());

    //output mode: pulse or pattern (also switches the board's output)
    pulseModeButton = std::make_unique<ColourButton> ("Pulse", FontOptions (14.0f));
    pulseModeButton->setTooltip ("One pulse train on every selected LED");
    pulseModeButton->addListener (this);
    addAndMakeVisible (pulseModeButton.get());
    patternModeButton = std::make_unique<ColourButton> ("Pattern", FontOptions (14.0f));
    patternModeButton->setTooltip ("Each LED plays its own waveform, from a pattern stored on the board");
    patternModeButton->addListener (this);
    addAndMakeVisible (patternModeButton.get());

    //trigger select (PC, EXTERNAL), shared by both modes
    triggerLabel = std::make_unique<Label> ("Trigger Label", "Trigger");
    triggerLabel->setJustificationType (Justification::centredRight);
    addAndMakeVisible (triggerLabel.get());
    triggerSelectBox = std::make_unique<ComboBox> ("Trigger Type");
    triggerSelectBox->addItem ("PC", 1);
    triggerSelectBox->addItem ("TTL IN 0", 2);
    triggerSelectBox->addListener (this);
    triggerSelectBox->setSelectedId (1, dontSendNotification);
    triggerSelectBox->setTooltip ("What starts the stimulation: the Start Stim button (PC) or a TTL input");
    addAndMakeVisible (triggerSelectBox.get());

    //upload: pulse settings, or the pattern
    uploadStimParamButton = std::make_unique<ColourButton> ("Upload pulse settings", FontOptions (14.0f));
    uploadStimParamButton->addListener (this);
    addAndMakeVisible (uploadStimParamButton.get());

    //LED selection under the probe (both modes): a range, all, none
    rangeLabel = std::make_unique<Label> ("Range Label", "LEDs");
    rangeLabel->setJustificationType (Justification::centredRight);
    addAndMakeVisible (rangeLabel.get());
    rangeEditor = std::make_unique<TextEditor> ("LED Range");
    rangeEditor->setTextToShowWhenEmpty ("e.g. 1-16, 33", Colours::grey);
    rangeEditor->setTooltip ("Type LED numbers or ranges (1-16, 33, 40-42) and press Enter to select them");
    rangeEditor->onReturnKey = [this] { applyRangeText(); };
    rangeEditor->onEscapeKey = [this] { updateRangeText (true); };
    addAndMakeVisible (rangeEditor.get());
    rangeSelectButton = std::make_unique<ColourButton> ("Select", FontOptions (13.0f));
    rangeSelectButton->setTooltip ("Select the LEDs typed in the box");
    rangeSelectButton->addListener (this);
    addAndMakeVisible (rangeSelectButton.get());
    allLedsButton = std::make_unique<ColourButton> ("All", FontOptions (13.0f));
    allLedsButton->addListener (this);
    addAndMakeVisible (allLedsButton.get());
    noLedsButton = std::make_unique<ColourButton> ("None", FontOptions (13.0f));
    noLedsButton->addListener (this);
    addAndMakeVisible (noLedsButton.get());
    selectionHint = std::make_unique<Label> ("Selection Hint");
    selectionHint->setFont (FontOptions (12.0f));
    addAndMakeVisible (selectionHint.get());

    //pulse mode settings
    pulseHeader = std::make_unique<Label> ("Pulse Header", "Pulse output");
    pulseHeader->setFont (FontOptions (15.0f, Font::bold));
    addAndMakeVisible (pulseHeader.get());

    for (const char* text : { "Amplitude (uA)", "Period (ms)", "Pulse width (ms)", "Rise/fall time", "Pulses" })
    {
        auto* l = pulseLabels.add (new Label ({}, String (text).replace ("uA", String::fromUTF8 ("\xc2\xb5" "A"))));
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }

    auto makeInput = [this] (const String& name)
    {
        auto ed = std::make_unique<TextEditor> (name);
        ed->setText ("0");
        ed->setJustification (Justification::centredRight);
        ed->setInputRestrictions (8, "0123456789");
        ed->onTextChange = [this] { updatePulsePreview(); };
        addAndMakeVisible (ed.get());
        return ed;
    };
    amplitudeInput = makeInput ("Amplitude");
    periodInput = makeInput ("Period");
    pulseWidthInput = makeInput ("Pulse Width");
    pulseCountInput = makeInput ("Pulse Count");

    edgeSelectBox = std::make_unique<ComboBox> ("Edge Select");
    edgeSelectBox->addItem ("0 ms", 1);
    edgeSelectBox->addItem ("0.1 ms", 2);
    edgeSelectBox->addItem ("0.5 ms", 3);
    edgeSelectBox->addItem ("1 ms", 4);
    edgeSelectBox->addItem ("2 ms", 5);
    edgeSelectBox->addListener (this);
    edgeSelectBox->setSelectedId (1, dontSendNotification);
    addAndMakeVisible (edgeSelectBox.get());

    pulseHelp = std::make_unique<Label> ("Pulse Help",
                                         "Select the LEDs that should pulse on the probe, then click Upload pulse settings. "
                                         "Start Stim (or the TTL trigger) plays it.");
    pulseHelp->setJustificationType (Justification::topLeft);
    addAndMakeVisible (pulseHelp.get());

    pulsePreview = std::make_unique<PatternEditor::WaveformPreview>();
    addAndMakeVisible (pulsePreview.get());
    pulseCycleButton = std::make_unique<PatternEditor::EditorButton> ("One cycle");
    pulseWholeButton = std::make_unique<PatternEditor::EditorButton> ("Whole");
    pulseCycleButton->onClick = [this]
    {
        pulsePreview->setWhole (false);
        updatePulsePreview();
    };
    pulseWholeButton->onClick = [this]
    {
        pulsePreview->setWhole (true);
        updatePulsePreview();
    };
    addAndMakeVisible (pulseCycleButton.get());
    addAndMakeVisible (pulseWholeButton.get());

    //pattern mode
    patternDoc = std::make_unique<PatternEditor::PatternDocument>();
    patternDoc->setProbe (PatternGen::ProbeMapping::FourShankChronic);
    patternDoc->addChangeListener (this);

    patternPanel = std::make_unique<PatternEditor::PatternPanel> (*patternDoc);
    patternPanel->onPatternReplaced = [this]
    {
        // the probe on the headstage wins over the one saved in the design
        patternDoc->setProbe ((PatternGen::ProbeMapping) jlimit (1, 4, probeTypeSelect->getSelectedId()));
    };
    addChildComponent (patternPanel.get());

    ledOverlay = std::make_unique<ProbeLedOverlay> (*this);
    addAndMakeVisible (ledOverlay.get());
    patternPanel->getPlayback().addChangeListener (this);

    //measures impedance (on acute configurations only).
    impedanceButton = std::make_unique<UtilityButton> ("Measure Impedances");
    impedanceButton->setRadius (3);
    impedanceButton->setBounds (280, 10, 145, 25);
    impedanceButton->setFont (FontOptions (14.0f));
    impedanceButton->addListener (this);
    //addAndMakeVisible (impedanceButton.get());

    saveImpedanceButton = std::make_unique<UtilityButton> ("Save Impedances");
    saveImpedanceButton->setRadius (3);
    saveImpedanceButton->setBounds (430, 10, 145, 25);
    saveImpedanceButton->setFont (FontOptions (14.0f));
    saveImpedanceButton->addListener (this);
    saveImpedanceButton->setEnabled (false);
    //addAndMakeVisible (saveImpedanceButton.get());

    setOutputMode (false);
    updatePulsePreview();
    updateBoardStatus();
    update();
}

void ChannelList::lookAndFeelChanged()
{
    const bool pattern = selectedStimOutputType != 0;
    const Colour text = findColour (ThemeColours::defaultText);
    const Colour on = findColour (ThemeColours::highlightedFill);
    const Colour off = findColour (ThemeColours::defaultFill);

    uploadStimParamButton->setColours (text, on);
    pulseModeButton->setColours (text, pattern ? off : on);
    patternModeButton->setColours (text, pattern ? on : off);
    for (auto* b : { rangeSelectButton.get(), allLedsButton.get(), noLedsButton.get() })
        b->setColours (text, off);

    for (auto* l : { triggerLabel.get(), pulseHeader.get(), pulseHelp.get(), rangeLabel.get(), selectionHint.get() })
        l->setColour (Label::textColourId, Colours::black);
    for (auto* l : pulseLabels)
        l->setColour (Label::textColourId, Colours::black);

    update();
}

void ChannelList::setOutputMode (bool pattern)
{
    selectedStimOutputType = pattern ? 1 : 0;
    if (board->foundInputSource())
        board->setStimOutputType (selectedStimOutputType);

    for (Component* c : { (Component*) pulseHeader.get(), (Component*) amplitudeInput.get(), (Component*) periodInput.get(),
                          (Component*) pulseWidthInput.get(), (Component*) edgeSelectBox.get(), (Component*) pulseCountInput.get(),
                          (Component*) pulseHelp.get(), (Component*) pulsePreview.get(), (Component*) pulseCycleButton.get(),
                          (Component*) pulseWholeButton.get() })
        c->setVisible (! pattern);
    for (auto* l : pulseLabels)
        l->setVisible (! pattern);

    patternPanel->setVisible (pattern);
    if (! pattern)
        patternPanel->getPlayback().stop();

    uploadStimParamButton->setLabel (pattern ? "Upload pattern" : "Upload pulse settings");
    uploadStimParamButton->setTooltip (pattern ? "Write the pattern to the board (if it changed) and set the repeat count"
                                               : "Send the pulse settings and selected LEDs to the board");
    selectionHint->setText (pattern ? "Click LEDs to select them, then apply a waveform. Blue = has a waveform."
                                    : "Click LEDs to select the ones that pulse (blue).",
                            dontSendNotification);

    lookAndFeelChanged();
    updateRangeText (true);
    resized();
    repaint();

    // the modes need different widths: let the viewport re-fit the content
    if (ownerCanvas != nullptr)
        ownerCanvas->resized();
}

int ChannelList::visibleLeds() const
{
    const int lanes = (shankCount == 1 ? 2 : 4);
    return jmin (64, lanes * 16);
}

Point<float> ChannelList::ledCentre (int index) const
{
    const int lane = index / 16;
    const int j = index % 16;
    const float yTop = kProbeY + 5.0f;
    const float yBottom = kProbeY + 470.0f;
    const float step = (yBottom - yTop) / 16.0f;
    return { kProbeX + lane * (kShankW + kShankGap) + kShankW * 0.5f - 2.0f, yBottom - (j + 0.5f) * step };
}

std::set<int> ChannelList::ledSelection() const
{
    return selectedStimOutputType != 0 ? patternDoc->selectedLeds() : pulseLeds;
}

void ChannelList::setLedSelection (std::set<int> leds)
{
    // only LEDs on the drawn probe
    for (auto it = leds.begin(); it != leds.end();)
        it = (*it < 1 || *it > visibleLeds()) ? leds.erase (it) : std::next (it);

    if (selectedStimOutputType != 0)
    {
        patternDoc->setSelectedBlocks ({});
        patternDoc->setSelectedLeds (leds);
    }
    else
    {
        pulseLeds = leds;
    }

    updateRangeText (false);
    ledOverlay->repaint();
}

bool ChannelList::ledHasWaveform (int led) const
{
    for (const auto& b : patternDoc->pattern().blocks)
        if (b.led == led)
            return true;
    return false;
}

void ChannelList::updateRangeText (bool evenIfEditing)
{
    if (! evenIfEditing && rangeEditor->hasKeyboardFocus (true))
        return;
    rangeEditor->setText (formatLedRange (ledSelection()), false);
    rangeEditor->setColour (TextEditor::outlineColourId, Colours::transparentBlack);
}

void ChannelList::applyRangeText()
{
    std::set<int> leds;
    if (! parseLedRange (rangeEditor->getText(), visibleLeds(), leds))
    {
        rangeEditor->setColour (TextEditor::outlineColourId, Colours::red);
        rangeEditor->repaint();
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Select LEDs",
                                          "Type LED numbers 1-" + String (visibleLeds()) + ", separated by commas, with a dash for ranges: e.g. 1-16, 33, 40-42");
        return;
    }
    setLedSelection (leds);
    updateRangeText (true);
}

void ChannelList::resized()
{
    const int W = getWidth();
    const int H = getHeight();

    // top bar: probe, mode, trigger, upload
    probeTypeSelect->setBounds (10, 10, 150, 26);
    pulseModeButton->setBounds (172, 10, 90, 26);
    patternModeButton->setBounds (264, 10, 90, 26);
    triggerLabel->setBounds (364, 10, 64, 26);
    triggerSelectBox->setBounds (430, 10, 100, 26);
    uploadStimParamButton->setBounds (544, 10, 190, 26);

    // LEDs on the probe drawing, and the selection row under it
    ledOverlay->setBounds (0, 0, kProbeColumnW, kSelectRowY - 4); // same coordinates as the drawing
    rangeLabel->setBounds (6, kSelectRowY, 44, 26);
    rangeEditor->setBounds (52, kSelectRowY, 200, 26);
    rangeSelectButton->setBounds (258, kSelectRowY, 64, 26);
    allLedsButton->setBounds (328, kSelectRowY, 54, 26);
    noLedsButton->setBounds (388, kSelectRowY, 60, 26);
    selectionHint->setBounds (10, kSelectRowY + 30, kProbeColumnW - 20, 18);
    juce::ignoreUnused (H);

    // settings beside the probe
    const int x0 = kProbeColumnW + 10;
    patternPanel->setBounds (x0, kTopBarH + 4, jmax (300, W - x0 - 10), jmax (300, H - kTopBarH - 14));

    pulseHeader->setBounds (x0, kTopBarH + 10, 300, 24);
    Component* fields[] = { amplitudeInput.get(), periodInput.get(), pulseWidthInput.get(), edgeSelectBox.get(), pulseCountInput.get() };
    for (int i = 0; i < 5; ++i)
    {
        const int y = kTopBarH + 42 + i * 32;
        pulseLabels[i]->setBounds (x0, y, 130, 26);
        fields[i]->setBounds (x0 + 136, y, 90, 26);
    }
    // help, then the preview under the settings: pulse mode stays narrow
    // enough to sit beside the LFP viewer
    const int pulseW = jlimit (260, kPulseColumnW, W - x0 - 20);
    int y = kTopBarH + 42 + 5 * 32 + 4;
    pulseHelp->setBounds (x0, y, pulseW, 62);
    y += 68;
    pulsePreview->setBounds (x0, y, pulseW, 160);
    y += 164;
    pulseCycleButton->setBounds (x0, y, 90, 24);
    pulseWholeButton->setBounds (x0 + 94, y, 70, 24);
}

int ChannelList::getMinimumWidth() const
{
    return selectedStimOutputType != 0 ? kMinWidth : kProbeColumnW + 10 + kPulseColumnW + 20;
}

void ChannelList::updatePulsePreview()
{
    // the board's pulse train: pulses of the width, one every period
    const int periodMs = periodInput->getText().getIntValue();
    const int widthMs = jmax (1, pulseWidthInput->getText().getIntValue());
    const int count = jmax (1, pulseCountInput->getText().getIntValue());
    const double period = jmax (widthMs, periodMs) / 1000.0;

    PatternGen::Block b;
    b.wave.kind = PatternGen::WaveKind::Shaped;
    auto& t = b.wave.shaped;
    t.shape = PatternGen::PulseShape::Square;
    t.amp = amplitudeInput->getText().getIntValue();
    t.width = widthMs / 1000.0;
    t.count = count;
    t.period = period;

    // one cycle = one period (pulse and the gap after it); whole = every pulse
    const bool whole = pulsePreview->showsWhole();
    b.length = whole ? count * period : period;
    pulsePreview->setBlock (b, b.length, whole ? String ("All " + String (count) + (count == 1 ? " pulse" : " pulses")) : String ("One cycle"));

    pulseCycleButton->setStyle (whole ? PatternEditor::EditorButton::Style::Normal : PatternEditor::EditorButton::Style::Accent);
    pulseWholeButton->setStyle (whole ? PatternEditor::EditorButton::Style::Accent : PatternEditor::EditorButton::Style::Normal);
}

void ChannelList::buttonClicked (Button* btn)
{
    if (btn == pulseModeButton.get())
    {
        setOutputMode (false);
        return;
    }
    if (btn == patternModeButton.get())
    {
        setOutputMode (true);
        return;
    }
    if (btn == rangeSelectButton.get())
    {
        applyRangeText();
        return;
    }
    if (btn == allLedsButton.get())
    {
        std::set<int> all;
        for (int i = 1; i <= visibleLeds(); ++i)
            all.insert (i);
        setLedSelection (all);
        return;
    }
    if (btn == noLedsButton.get())
    {
        setLedSelection ({});
        return;
    }

    if (btn == uploadStimParamButton.get())
    {
        LOGD ("UPLOAD STIM PARAMS PRESSED")

        if (selectedStimOutputType != 0)
        {
            uploadPattern();
            return;
        }

        if (! board->foundInputSource())
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload", "No HectoSTAR controller is connected.");
            return;
        }

        board->setStimOutputType (0);

        LOGD ("Pulse Output Selected")
        //get all text entries for waveform at stim
        amplitude = amplitudeInput->getTextValue().toString().getIntValue();
        period = periodInput->getTextValue().toString().getIntValue();
        pulse_width = pulseWidthInput->getTextValue().toString().getIntValue();
        edge_sel = edgeSelectBox->getComponentID().getIntValue();
        pulse_count = pulseCountInput->getTextValue().toString().getIntValue();

        //selected LEDs (drawing position i = LED i + 1) -> board channels
        int leds[64];
        for (int i = 0; i < 64; ++i)
            leds[ledMapping[i]] = pulseLeds.count (i + 1) > 0 ? 1 : 0;

        //trig source is 0 indexed on fpga
        board->uploadStimParameters (amplitude, period, pulse_width, edge_sel, pulse_count, (trig_source - 1), leds);
    }
}

void ChannelList::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == &patternPanel->getPlayback())
    {
        ledOverlay->repaint();
        return;
    }

    updateBoardStatus();
    if (selectedStimOutputType != 0)
        updateRangeText (false);
    ledOverlay->repaint();
}

void ChannelList::updateBoardStatus()
{
    if (patternPanel == nullptr)
        return;

    const int repeats = patternDoc->pattern().repeatCount;

    if (! board->foundInputSource())
        patternPanel->setBoardStatus ("No board connected", false);
    else if (flashVersion == 0)
        patternPanel->setBoardStatus ("Not uploaded yet", false);
    else if (flashVersion != patternDoc->contentVersion())
        patternPanel->setBoardStatus ("Changed since the last upload", false);
    else if (flashRepeats != repeats)
        patternPanel->setBoardStatus ("Upload to apply the new repeat count", false);
    else
        patternPanel->setBoardStatus ("On the board: this pattern", true);
}

void ChannelList::uploadPattern()
{
    if (! board->foundInputSource())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload pattern", "No HectoSTAR controller is connected.");
        return;
    }

    const auto version = patternDoc->contentVersion();
    const int repeats = patternDoc->pattern().repeatCount;

    Component::SafePointer<ChannelList> safeThis (this);
    auto ready = [safeThis, version, repeats]
    {
        if (safeThis == nullptr)
            return;
        safeThis->board->setStimOutputType (1);
        safeThis->board->setPatternStimRepeatCount (repeats - 1);
        safeThis->flashVersion = version;
        safeThis->flashRepeats = repeats;
        safeThis->updateBoardStatus();
    };

    // flash already holds this pattern: only the repeat count changes
    if (version == flashVersion)
    {
        ready();
        return;
    }

    patternPanel->prepareOutput ("Upload pattern", [safeThis, ready] (std::vector<std::uint8_t> bytes)
                                 {
                                     if (safeThis == nullptr)
                                         return;
                                     const File f = File::getSpecialLocation (File::tempDirectory).getChildFile ("HectoSTAR_pattern.bin");
                                     f.deleteFile();
                                     if (! f.appendData (bytes.data(), bytes.size()))
                                     {
                                         AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload pattern", "Couldn't write " + f.getFullPathName());
                                         return;
                                     }
                                     safeThis->beginPatternUpload (f, [ready] (bool ok)
                                                                   {
                                                                       if (ok)
                                                                           ready();
                                                                   });
                                 });
}

void ChannelList::beginPatternUpload (const File& file, std::function<void (bool)> afterUpload)
{
    if (! board->foundInputSource())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload pattern", "No HectoSTAR controller is connected.");
        return;
    }

    const String fileName = file.getFileName();

    Component::SafePointer<ChannelList> safeThis (this);

    // Upload in the background (owned by the DeviceThread); the GUI stays usable.
    const bool started = board->startPatternUpload (file, [safeThis, fileName, afterUpload] (bool ok)
                                                    {
                                                        if (! ok)
                                                            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload failed",
                                                                                              "The pattern could not be written to the board (" + fileName + ").");

                                                        if (safeThis != nullptr)
                                                        {
                                                            safeThis->uploadWindow.reset();
                                                            safeThis->uploadStimParamButton->setEnabled (true);
                                                        }

                                                        if (afterUpload)
                                                            afterUpload (ok);
                                                    });

    if (started)
    {
        // block re-entry until it finishes
        uploadStimParamButton->setEnabled (false);
        uploadWindow = std::make_unique<PatternUploadWindow> (board, file, this);
    }
    else
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Upload pattern", "A pattern upload is already in progress.");
    }
}

void ChannelList::disableAll()
{
    uploadStimParamButton->setEnabled (false);
}

void ChannelList::enableAll()
{
    uploadStimParamButton->setEnabled (! board->isPatternUploadRunning());
}

void ChannelList::comboBoxChanged (ComboBox* b)
{
    //pulse waveform trigger selection.
    if (b == triggerSelectBox.get())
    {
        trig_source = b->getSelectedId();
    }
    else if (b == probeTypeSelect.get())
    {
        const int probeGeometry = b->getSelectedId();

        const int* mapping = four_Shank_Chronic_mapping;
        shankCount = 4;
        buttonCountPerShank = 16;

        if (probeGeometry == 2) // 2-Shank Chronic
        {
            shankCount = 2;
            buttonCountPerShank = 32;
            mapping = two_Shank_Chronic_mapping;
        }
        else if (probeGeometry == 3) // 4 Shank Acute
            mapping = four_Shank_Acute_mapping;
        else if (probeGeometry == 4) // Demo Board
            mapping = demo_Board_mapping;

        for (int i = 0; i < 64; i++)
            ledMapping[i] = mapping[i];

        patternDoc->setProbe ((PatternGen::ProbeMapping) jlimit (1, 4, probeGeometry));
        setLedSelection (ledSelection()); // drop LEDs no longer drawn
        update();
    }
}

void ChannelList::update()
{
    if (! board->foundInputSource())
    {
        disableAll();
        updateBoardStatus();
        repaint();
        return;
    }

    staticLabels.clear();
    channelComponents.clear();
    uploadStimParamButton->setEnabled (! board->isPatternUploadRunning());

    updateBoardStatus();
    repaint();
}

void ChannelList::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::grey);
    g.setColour (juce::Colours::black);

    const float strokeW = 2.0f;
    const float gap = strokeW * 8.0f;

    // Fixed geometry (shared with ledCentre)
    const float startX = kProbeX;
    const float startY = kProbeY;

    const float shankW = kShankW;
    const float shankH = kShankH;

    const int lanesVisible = (shankCount == 1 ? 2 : 4);

    // 32 electrodes per drawn lane/block
    const int electrodesPerLane = 32;
    const float electrodeSize = 7.0f;
    const int positionsAlongY = electrodesPerLane / 2;
    const float electrodeSpacing = (shankH / (float) positionsAlongY) - 3.0f;

    auto laneLabel = [this] (int lane) -> juce::String
    {
        if (shankCount == 4)
            return "S" + juce::String (lane + 1);

        if (shankCount == 2)
        {
            static const char* names[] = { "S1B", "S1T", "S2B", "S2T" };
            return names[lane];
        }

        // shankCount == 1
        static const char* names[] = { "S1B", "S1T" };
        return names[lane];
    };

    auto laneHasPointyTip = [this] (int lane) -> bool
    {
        if (shankCount == 4)
            return true; // all pointy

        if (shankCount == 2)
            return (lane == 0 || lane == 2); // only bottom halves

        return (lane == 0); // 1-shank: only S1B
    };

    auto laneNeedsBreakAtTop = [this] (int lane) -> bool
    {
        if (shankCount == 2)
            return (lane == 0 || lane == 2); // top of S1B/S2B

        if (shankCount == 1)
            return (lane == 0); // top of S1B

        return false;
    };

    auto laneNeedsBreakAtBottom = [this] (int lane) -> bool
    {
        if (shankCount == 2)
            return (lane == 1 || lane == 3); // bottom of S1T/S2T

        if (shankCount == 1)
            return (lane == 1); // bottom of S1T

        return false;
    };

    auto drawBreakLine = [&g] (float x, float y, float w)
    {
        juce::Path p;
        p.startNewSubPath (x, y);
        p.lineTo (x + w, y);

        juce::Path dashed;
        const float dashes[] = { 1.0f, 1.0f };
        juce::PathStrokeType (5.0f).createDashedStroke (dashed, p, dashes, 2);

        g.strokePath (dashed, juce::PathStrokeType (5.0f));
    };

    auto drawFlatShank = [&g, strokeW] (float x, float y, float w, float h)
    {
        g.drawRect (juce::Rectangle<float> (x, y, w, h), strokeW);
    };

    auto drawPointyShank = [&g, strokeW] (float x, float y, float w, float h)
    {
        juce::Path outline;
        outline.startNewSubPath (x - 5.0f, y);
        outline.lineTo (x, y + h - (w * 0.5f));
        outline.lineTo (x + w / 2.0f, y + h);
        outline.lineTo (x + w, y + h - (w * 0.5f));
        outline.lineTo (x + w, y);
        outline.closeSubPath();

        g.strokePath (outline, juce::PathStrokeType (strokeW));
    };

    // Converts the current drawing index j into a 1-based electrode position
    // ordered from top-to-bottom, with top-left = 1, top-right = 2, etc.
    auto topToBottomIndex = [positionsAlongY] (int j) -> int
    {
        const bool isLeft = (j % 2 == 0);

        const int pairFromBottom = j / 2;
        const int rowFromTop = (positionsAlongY - 1) - pairFromBottom;

        return rowFromTop * 2 + (isLeft ? 1 : 2);
    };

    auto electrodeNumberForLane = [this, electrodesPerLane, topToBottomIndex] (int lane, int j) -> int
    {
        const int n = topToBottomIndex (j); // 1..32, top-to-bottom within lane

        if (shankCount == 4)
        {
            // S1:  1..32
            // S2:  33..64
            // S3:  65..96
            // S4:  97..128
            return lane * electrodesPerLane + n;
        }

        if (shankCount == 2)
        {
            // lanes:
            // 0 = S1B
            // 1 = S1T
            // 2 = S2B
            // 3 = S2T
            switch (lane)
            {
                case 0:
                    return 32 + n; // S1B: 33..64, top-to-bottom
                case 1:
                    return n; // S1T: 1..32, top-to-bottom

                case 2:
                    return 96 + n; // S2B: 128..97, top-to-bottom
                case 3:
                    return 64 + n; // S2T: 96..65, top-to-bottom

                default:
                    break;
            }
        }

        if (shankCount == 1)
        {
            // lanes:
            // 0 = S1B
            // 1 = S1T
            switch (lane)
            {
                case 0:
                    return 32 + n; // S1B: 33..64, top-to-bottom
                case 1:
                    return n; // S1T: 1..32, top-to-bottom

                default:
                    break;
            }
        }

        return n;
    };

    for (int lane = 0; lane < lanesVisible; ++lane)
    {
        const float x = startX + lane * (shankW + gap);
        const float y = startY;

        g.setColour (juce::Colours::black);

        if (laneHasPointyTip (lane))
            drawPointyShank (x, y, shankW, shankH);
        else
            drawFlatShank (x, y, shankW, shankH);

        if (laneNeedsBreakAtTop (lane))
            drawBreakLine (x, y, shankW);

        if (laneNeedsBreakAtBottom (lane))
            drawBreakLine (x, y + shankH, shankW);

        g.setFont (14.0f);
        g.drawText (laneLabel (lane),
                    (int) x,
                    (int) (y - 22.0f),
                    (int) shankW,
                    20,
                    juce::Justification::centredTop);

        g.setFont (10.0f);

        for (int j = 0; j < electrodesPerLane; ++j)
        {
            const bool isLeft = (j % 2 == 0);

            const int pair = j / 2;
            const int pairReversed = (positionsAlongY - 1) - pair;

            const float ex = isLeft
                                 ? (x + electrodeSize + 10.0f)
                                 : (x + shankW - electrodeSize * 2.0f - 10.0f);

            const float ey = y
                             + pairReversed * electrodeSpacing
                             + (isLeft ? 0.0f : electrodeSpacing / 2.0f)
                             + 25.0f;

            g.setColour (juce::Colours::black);
            g.fillRect (ex, ey, electrodeSize, electrodeSize);

            const int electrodeNumber = electrodeNumberForLane (lane, j);

            g.drawText ("E" + juce::String (electrodeNumber),
                        (int) (ex - 12.0f), 
                        (int) (ey - 12.0f),
                        34,
                        12,
                        juce::Justification::centredLeft);
        }
    } 

    // Disclaimer below probe
    const int disclaimerX = 30;
    const int disclaimerW = juce::jmax (200, getWidth() - 60);

    const int disclaimerY1 = kSelectRowY + 56;
    const int disclaimerY2 = disclaimerY1 + 20;

    g.setColour (juce::Colours::black);
    g.setFont (12.0f);

    g.drawText ("NOTE: Probe overlay is an approximate visual aid. Please refer to the ",
                disclaimerX,
                disclaimerY1,
                disclaimerW,
                14,
                juce::Justification::left);

    g.drawText ("specific probe geometry for exact LED and Electrodes spacing/location.",
                disclaimerX,
                disclaimerY2,
                disclaimerW,
                14,
                juce::Justification::left);
}