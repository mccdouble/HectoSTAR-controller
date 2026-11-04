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

Colour ledColour() { return Colour (0xff4169e1); } // royal blue, like the pulse-mode ticks

} // namespace

/** Pattern mode: draws the LEDs on the probe (filled when they have a
    waveform, ringed when selected) and selects them: click, Ctrl/Cmd-click
    to toggle, Shift-click to add, drag across LEDs to add them. */
class ChannelList::ProbeLedOverlay : public Component,
                                     public TooltipClient,
                                     private ChangeListener
{
public:
    ProbeLedOverlay (ChannelList& owner_, PatternEditor::PatternDocument& d) : owner (owner_), doc (d)
    {
        doc.addChangeListener (this);
    }

    ~ProbeLedOverlay() override { doc.removeChangeListener (this); }

    void paint (Graphics& g) override
    {
        std::set<int> used;
        for (int led : PatternGen::activeLeds (doc.pattern()))
            used.insert (led);

        for (int i = 0; i < owner.visibleLeds(); ++i)
        {
            const int led = i + 1;
            const auto c = owner.ledCentre (i);
            const juce::Rectangle<float> r (c.x - 9.0f, c.y - 9.0f, 18.0f, 18.0f);
            const bool isSel = doc.selectedLeds().count (led) > 0;

            if (isSel)
            {
                g.setColour (Colours::white);
                g.fillEllipse (r.expanded (4.0f));
            }

            g.setColour (used.count (led) > 0 ? ledColour() : Colours::white.withAlpha (0.35f));
            g.fillEllipse (r);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.drawEllipse (r, isSel ? 2.0f : 1.2f);
        }

        g.setColour (Colours::black);
        g.setFont (FontOptions (12.0f));
        g.drawText ("Click LEDs to select them (Ctrl-click or drag for more). Blue = has a waveform.",
                    10, (int) (kProbeY + kShankH + 8), kProbeColumnW - 20, 16, Justification::centredLeft);
    }

    void mouseDown (const MouseEvent& e) override
    {
        const int led = ledAt (e.position);
        auto sel = doc.selectedLeds();
        const auto mods = e.mods;

        if (led == 0)
        {
            if (! mods.isCommandDown() && ! mods.isShiftDown())
                select ({});
            dragAdds = false;
            return;
        }

        if (mods.isCommandDown())
        {
            if (sel.count (led) > 0)
                sel.erase (led);
            else
                sel.insert (led);
        }
        else if (mods.isShiftDown())
            sel.insert (led);
        else
            sel = { led };

        dragAdds = true;
        select (sel);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragAdds)
            return;
        const int led = ledAt (e.position);
        if (led != 0 && doc.selectedLeds().count (led) == 0)
        {
            auto sel = doc.selectedLeds();
            sel.insert (led);
            select (sel);
        }
    }

    void mouseMove (const MouseEvent& e) override { hovered = ledAt (e.position); }

    String getTooltip() override
    {
        if (hovered == 0)
            return {};
        const int row = PatternEditor::rowFromBottom (hovered) + 1;
        return "LED " + String (hovered) + " (shank " + String (PatternEditor::shankOf (hovered)) + ", " + String (row)
               + (row == 1 ? "st" : row == 2 ? "nd" : row == 3 ? "rd" : "th") + " from the tip)";
    }

    bool hitTest (int, int y) override { return y >= kTopBarH; } // leave the top bar to its controls

private:
    ChannelList& owner;
    PatternEditor::PatternDocument& doc;
    bool dragAdds = false;
    int hovered = 0;

    void changeListenerCallback (ChangeBroadcaster*) override { repaint(); }

    void select (std::set<int> leds)
    {
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds (std::move (leds));
    }

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
    pulseModeButton->setTooltip ("One pulse train on every ticked LED");
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
                                         "Tick the LEDs on the probe that should pulse, then click Upload pulse settings. "
                                         "Start Stim (or the TTL trigger) plays it.");
    pulseHelp->setJustificationType (Justification::topLeft);
    addAndMakeVisible (pulseHelp.get());

    pulsePreview = std::make_unique<PatternEditor::WaveformPreview>();
    pulsePreview->setWhole (true);
    addAndMakeVisible (pulsePreview.get());

    //Initialize the led toggle buttons
    for (int i = 0; i < 64; ++i)
    {
        toggleButtons[i] = std::make_unique<ToggleButton> ("");
        toggleButtons[i]->setClickingTogglesState (true);
        toggleButtons[i]->setTriggeredOnMouseDown (true);
        toggleButtons[i]->setWantsKeyboardFocus (false);

        toggleButtons[i]->addListener (this);
        addAndMakeVisible (toggleButtons[i].get());
    }

    //pattern mode
    patternDoc = std::make_unique<PatternEditor::PatternDocument>();
    patternDoc->setProbe (PatternGen::ProbeMapping::FourShankChronic);
    patternDoc->addChangeListener (this);

    patternPanel = std::make_unique<PatternEditor::PatternPanel> (*patternDoc);
    patternPanel->onUploadBinFile = [this] (const File& f) { uploadPatternFile (f); };
    patternPanel->onPatternReplaced = [this]
    {
        // the probe on the headstage wins over the one saved in the design
        patternDoc->setProbe ((PatternGen::ProbeMapping) jlimit (1, 4, probeTypeSelect->getSelectedId()));
    };
    addChildComponent (patternPanel.get());

    ledOverlay = std::make_unique<ProbeLedOverlay> (*this, *patternDoc);
    addChildComponent (ledOverlay.get());

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

    for (int i = 0; i < 64; ++i)
    {
        toggleButtons[i]->setColour (ToggleButton::ColourIds::tickDisabledColourId, Colours::darkgrey);
        toggleButtons[i]->setColour (ToggleButton::ColourIds::tickColourId, ledColour());
    }

    for (auto* l : { triggerLabel.get(), pulseHeader.get(), pulseHelp.get() })
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
                          (Component*) pulseHelp.get(), (Component*) pulsePreview.get() })
        c->setVisible (! pattern);
    for (auto* l : pulseLabels)
        l->setVisible (! pattern);

    for (int i = 0; i < 64; ++i)
        toggleButtons[i]->setVisible (! pattern && i < visibleLeds());

    patternPanel->setVisible (pattern);
    ledOverlay->setVisible (pattern);

    uploadStimParamButton->setLabel (pattern ? "Upload pattern" : "Upload pulse settings");
    uploadStimParamButton->setTooltip (pattern ? "Write the pattern to the board (if it changed) and set the repeat count"
                                               : "Send the pulse settings and ticked LEDs to the board");

    lookAndFeelChanged();
    resized();
    repaint();
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

    // LEDs on the probe drawing
    for (int i = 0; i < 64; ++i)
    {
        const auto c = ledCentre (i);
        toggleButtons[i]->setBounds ((int) std::round (c.x - 10.0f), (int) std::round (c.y - 10.0f), 20, 20);
    }
    ledOverlay->setBounds (0, 0, kProbeColumnW, H); // same coordinates as the drawing

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
    const int previewX = x0 + 250;
    pulsePreview->setBounds (previewX, kTopBarH + 42, jlimit (260, 520, W - previewX - 20), 154);
    pulseHelp->setBounds (x0, kTopBarH + 42 + 5 * 32 + 6, jmin (700, W - x0 - 20), 40);
}

void ChannelList::updatePulsePreview()
{
    PatternGen::Block b;
    b.wave.kind = PatternGen::WaveKind::Shaped;
    auto& t = b.wave.shaped;
    t.shape = PatternGen::PulseShape::Square;
    t.amp = amplitudeInput->getText().getIntValue();
    t.width = jmax (1, pulseWidthInput->getText().getIntValue()) / 1000.0;
    t.count = jmax (1, pulseCountInput->getText().getIntValue());
    const int periodMs = periodInput->getText().getIntValue();
    t.rate = periodMs > 0 ? 1000.0 / periodMs : 1.0;
    b.length = PatternGen::naturalLength (b.wave);
    pulsePreview->setBlock (b);
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

        //check the enabled buttons for which LEDs to output to
        int leds[64];
        for (int i = 0; i < 64; ++i)
            leds[ledMapping[i]] = toggleButtons[i]->getToggleState() ? 1 : 0;

        //trig source is 0 indexed on fpga
        board->uploadStimParameters (amplitude, period, pulse_width, edge_sel, pulse_count, (trig_source - 1), leds);
    }
}

String ChannelList::patternKey() const
{
    // what goes into flash: everything but the repeat count
    PatternGen::Pattern p = patternDoc->pattern();
    p.repeatCount = 1;
    return String::fromUTF8 (PatternGen::toJson (p).c_str());
}

void ChannelList::changeListenerCallback (ChangeBroadcaster*)
{
    updateBoardStatus();
}

void ChannelList::updateBoardStatus()
{
    if (patternPanel == nullptr)
        return;

    const int repeats = patternDoc->pattern().repeatCount;

    if (! board->foundInputSource())
        patternPanel->setBoardStatus ("No board connected", false);
    else if (flashFileName.isNotEmpty())
        patternPanel->setBoardStatus ("On the board: " + flashFileName, false);
    else if (flashKey.isEmpty())
        patternPanel->setBoardStatus ("Not uploaded yet", false);
    else if (flashKey != patternKey())
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

    const String key = patternKey();
    const int repeats = patternDoc->pattern().repeatCount;

    Component::SafePointer<ChannelList> safeThis (this);
    auto ready = [safeThis, key, repeats]
    {
        if (safeThis == nullptr)
            return;
        safeThis->board->setStimOutputType (1);
        safeThis->board->setPatternStimRepeatCount (repeats - 1);
        safeThis->flashKey = key;
        safeThis->flashFileName.clear();
        safeThis->flashRepeats = repeats;
        safeThis->updateBoardStatus();
    };

    // flash already holds this pattern: only the repeat count changes
    if (key == flashKey && flashFileName.isEmpty())
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

void ChannelList::uploadPatternFile (const File& file)
{
    const int repeats = patternDoc->pattern().repeatCount;
    Component::SafePointer<ChannelList> safeThis (this);

    beginPatternUpload (file, [safeThis, repeats, name = file.getFileName()] (bool ok)
                        {
                            if (! ok || safeThis == nullptr)
                                return;
                            safeThis->board->setStimOutputType (1);
                            safeThis->board->setPatternStimRepeatCount (repeats - 1);
                            safeThis->flashKey.clear();
                            safeThis->flashFileName = name;
                            safeThis->flashRepeats = repeats;
                            safeThis->updateBoardStatus();
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
    const int64 fileSize = file.getSize();

    Component::SafePointer<ChannelList> safeThis (this);

    // Upload in the background (owned by the DeviceThread); the GUI stays usable.
    const bool started = board->startPatternUpload (file, [safeThis, fileName, fileSize, afterUpload] (bool ok)
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

    for (int i = 0; i < 64; ++i)
        toggleButtons[i]->setEnabled (false);
}

void ChannelList::enableAll()
{
    uploadStimParamButton->setEnabled (! board->isPatternUploadRunning());

    for (int i = 0; i < 64; ++i)
        toggleButtons[i]->setEnabled (true);
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

        for (int i = 0; i < 64; ++i)
            toggleButtons[i]->setVisible (selectedStimOutputType == 0 && i < visibleLeds());

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

    for (int i = 0; i < 64; ++i)
        toggleButtons[i]->setEnabled (i < visibleLeds());

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

    const int disclaimerY1 = (int) std::round (startY + shankH + 30.0f);
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