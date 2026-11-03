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

using namespace RhythmNode;

ChannelList::ChannelList (DeviceThread* board_, DeviceEditor* editor_) : board (board_), editor (editor_), maxChannels (0)
{
    channelComponents.clear();

    //Probe Geometry Selection 


    //label for stim output selection (Pulse or Pattern).
    probeTypeSelect = std::make_unique<ComboBox> ("Probe Select");
    probeTypeSelect->addItem ("4-Shank Chronic", 1);
    probeTypeSelect->addItem ("2-Shank Chronic", 2);
    probeTypeSelect->addItem ("4-Shank Acute", 3);
    probeTypeSelect->addItem ("Demo Board", 4);

    //add in future, all other possible configs.

    probeTypeSelect->setBounds (10, 10, 100, 25);
    probeTypeSelect->addListener (this);
    probeTypeSelect->setSelectedId (1, dontSendNotification);
    addAndMakeVisible (probeTypeSelect.get());

    //Output Selection Button
    outputTypeSelect = std::make_unique<ComboBox> ("Output Selection");
    outputTypeSelect->addItem ("Pulse Output", 1);
    outputTypeSelect->addItem ("Pattern Output", 2);
    outputTypeSelect->setBounds (125, 10, 140, 25);
    outputTypeSelect->addListener (this);
    outputTypeSelect->setSelectedId (1, dontSendNotification);
    addAndMakeVisible (outputTypeSelect.get());
    selectedStimOutputType = 0; //set default output.

    //upload stim parameters
    uploadStimParamButton = std::make_unique<ColourButton> ("Upload Stim Parameters", FontOptions (14.0f));
    uploadStimParamButton->setBounds (270, 10, 170, 25);
    uploadStimParamButton->addListener (this);
    addAndMakeVisible (uploadStimParamButton.get());

    
    selectPatternFileButton = std::make_unique<ColourButton> ("Select Pattern Stim File", FontOptions (14.0f));
    selectPatternFileButton->setBounds (445, 10, 170, 25);
    selectPatternFileButton->addListener (this);
    addAndMakeVisible (selectPatternFileButton.get());

    //pulse count
    patternRepeatCount = std::make_unique<TextEditor> ("Pattern Repeat Count");
    patternRepeatCount->setText ("1");
    patternRepeatCount->setBounds (625, 10, 40, 25);
    //patternRepeatCount->addListener(this);
    addAndMakeVisible (patternRepeatCount.get());

    //count label
    //waveform input labels
    repeatLabel = std::make_unique<Label> ("Repeat Label", "Count");
    repeatLabel->setEditable (false);
    repeatLabel->setBounds (675, 10, 200, 25);
    addAndMakeVisible (repeatLabel.get());

    //waveform input labels
    waveformInputLabel = std::make_unique<Label> ("Waveform Labels", "Amplitude(uA)    Period(ms)     Pulse Width(ms)    Rise/Fall Time    #Pulses   Trigger Type");
    waveformInputLabel->setEditable (false);
    waveformInputLabel->setBounds (10, 50, 800, 25);
    addAndMakeVisible (waveformInputLabel.get());

    //amplitude
    amplitudeInput = std::make_unique<TextEditor> ("Amplitude");
    amplitudeInput->setText ("0");
    amplitudeInput->setBounds (30, 75, 50, 25);
    addAndMakeVisible (amplitudeInput.get());

    //period
    periodInput = std::make_unique<TextEditor> ("Period");
    periodInput->setText ("0");
    periodInput->setBounds (150, 75, 50, 25);
    addAndMakeVisible (periodInput.get());

    //pulse width
    pulseWidthInput = std::make_unique<TextEditor> ("Pulse Width");
    pulseWidthInput->setText ("0");
    pulseWidthInput->setBounds (280, 75, 50, 25);
    addAndMakeVisible (pulseWidthInput.get());

    //edgeSelect
    edgeSelectBox = std::make_unique<ComboBox> ("Edge Select");
    edgeSelectBox->addItem ("0 ms", 1);
    edgeSelectBox->addItem ("0.1 ms", 2);
    edgeSelectBox->addItem ("0.5 ms", 3);
    edgeSelectBox->addItem ("1 ms", 4);
    edgeSelectBox->addItem ("2 ms", 5);
    edgeSelectBox->setBounds (400, 75, 80, 25);
    edgeSelectBox->addListener (this);
    edgeSelectBox->setSelectedId (1, dontSendNotification);
    addAndMakeVisible (edgeSelectBox.get());

    //pulse count
    pulseCountInput = std::make_unique<TextEditor> ("Pulse Count");
    pulseCountInput->setText ("0");
    pulseCountInput->setBounds (530, 75, 50, 25);
    addAndMakeVisible (pulseCountInput.get());

    //trigger select (PC, EXTERNAL)
    triggerSelectBox = std::make_unique<ComboBox> ("Trigger Type");
    triggerSelectBox->addItem("PC", 1);
    triggerSelectBox->addItem("TTL IN 0", 2);
    //triggerSelectBox->addItem("TTL IN 1", 2);
    //triggerSelectBox->addItem("TTL IN 2", 3);
    //triggerSelectBox->addItem("TTL IN 3", 4);
    triggerSelectBox->setBounds(610, 75, 80, 25);
    triggerSelectBox->addListener(this);
    triggerSelectBox->setSelectedId(1, dontSendNotification);
    addAndMakeVisible (triggerSelectBox.get());

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

    patternOutputLabel = std::make_unique<Label> ("Output Type Alert");
    patternOutputLabel->setText ("Pattern Output is Selected!!", juce::dontSendNotification);
    patternOutputLabel->setJustificationType (juce::Justification::centred);
    patternOutputLabel->setBounds (112, 300, 200, 30);
    addAndMakeVisible(patternOutputLabel.get()); 
    patternOutputLabel->setVisible (false); // Starts hidden!

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

    update();
}

void ChannelList::lookAndFeelChanged()
{   

    //probably put the color selections here, make the default colors changed upon the theme color changing from editior,   

    //outputTypeLabel->setColour (Label::textColourId, findColour (ThemeColours::defaultText));
    uploadStimParamButton->setColours (findColour (ThemeColours::defaultText), findColour (ThemeColours::highlightedFill));  
    selectPatternFileButton->setColours (findColour (ThemeColours::defaultText), findColour (ThemeColours::defaultFill));
  

    for (int i = 0; i < 64; ++i)
    {
        toggleButtons[i]->setColour (ToggleButton::ColourIds::tickDisabledColourId, Colours::darkgrey);
        toggleButtons[i]->setColour (ToggleButton::ColourIds::tickColourId, Colours::royalblue);
    }

    //alert window
    patternOutputLabel->setColour (juce::Label::textColourId, juce::Colours::white);
    patternOutputLabel->setColour (juce::Label::backgroundColourId, juce::Colours::darkred);
    patternOutputLabel->setFont (juce::Font (16.0f, juce::Font::bold));

    //numberingSchemeLabel->setColour (Label::textColourId, findColour (ThemeColours::defaultText));


    update();
}

void ChannelList::buttonClicked (Button* btn)
{
  
    
    //UPLOAD ENABLED LEDS AND WAVEFORM PARAMETERS
    //update the LED toggle buttons

    for (int i = 0; i < 64; ++i)
    {
        if (btn == toggleButtons[i].get())
        {
            // Handle button click
            bool isToggled = toggleButtons[i]->getToggleState();
        }
    }

    if (btn == uploadStimParamButton.get())
    {
        //upload the stimulation parameters saved in the
        //This structure should be updated upon       //any change to the UI elements.
        LOGD ("UPLOAD STIM PARAMS PRESSED")

        //set which output type to use
        board->setStimOutputType(selectedStimOutputType);

        //pulse stim selected
        if (!selectedStimOutputType)
        {
            LOGD ("Pulse Output Selected")
            //get all text entries for waveform at stim
            amplitude = amplitudeInput->getTextValue().toString().getIntValue();
            period = periodInput->getTextValue().toString().getIntValue();
            pulse_width = pulseWidthInput->getTextValue().toString().getIntValue();
            edge_sel = edgeSelectBox->getComponentID().getIntValue();
            pulse_count = pulseCountInput->getTextValue().toString().getIntValue();

            //check the enabled buttons for which LEDs to output to

            int enabledLEDS[64];
            for (int i = 0; i < 64; ++i)
            {
                if (toggleButtons[i]->getToggleState())
                {
                    enabledLEDS[ledMapping[i]] = 1;
                }
                else
                {
                    enabledLEDS[ledMapping[i]] = 0;
                }
            }

            //trig source is 0 indexed on fpga
            board->uploadStimParameters (amplitude, period, pulse_width, edge_sel, pulse_count, (trig_source-1), enabledLEDS);
        }
        //pattern stim is selected
        else
        {
            LOGD ("Pattern Output Selected")
            repeatCount = (patternRepeatCount->getTextValue().toString().getIntValue()) - 1;
            board->setPatternStimRepeatCount(repeatCount);
        }

        //TEMP BUTTON TO SET THE REPEAT COUNT
    }
    //PATTERN STIMULATION BUTTON
    if (btn == selectPatternFileButton.get())
    {
        LOGD ("SELECT FILE BUTTON CLICKED")

        FileChooser fileChooser ("Please select a stimulation pattern file...");

        //opens documents folder
        if (fileChooser.browseForFileToOpen())

        {
            File chosenFile = fileChooser.getResult();
            String filename = chosenFile.getFileName();
            int64_t fileLength = chosenFile.getSize();

            String message = "File Name: " + filename + "\n" + "File Length: " + juce::String (fileLength) + " bytes";
            AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, "Uploading. Press Ok and Wait for Success Message", message);

            // Do the upload
            int uploaded = board->uploadPatternFile(chosenFile);

            if (uploaded >= 0)
                message = "File upload completed!\nFile Name: " + filename + "\nFile Length: " + juce::String (fileLength) + " bytes";
            else
                message = "File upload FAILED!\nFile Name: " + filename;

            AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, (uploaded >= 0) ? "Upload Successful" : "Upload Failed", message);
        }
    }

    /*
    if (btn == impedanceButton.get())
    {
        editor->measureImpedance();
        saveImpedanceButton->setEnabled (true);
    }
    else if (btn == saveImpedanceButton.get())
    {
        FileChooser chooseOutputFile ("Please select the location to save...",
                                      File(),
                                      "*.xml");

        if (chooseOutputFile.browseForFileToSave (true))
        {
            // Use the selected file
            File impedenceFile = chooseOutputFile.getResult();
            editor->saveImpedance (impedenceFile);
        }
    }
    */
    
}

void ChannelList::disableAll()
{

    uploadStimParamButton->setEnabled (false);

    //Initialize the led toggle buttons
    for (int i = 0; i < 64; ++i)
    {
        toggleButtons[i]->setEnabled (false);
    }
}

void ChannelList::enableAll()
{
    uploadStimParamButton->setEnabled (true);
    selectPatternFileButton->setEnabled (true);

    //Initialize the led toggle buttons
    for (int i = 0; i < 64; ++i)
    {
        toggleButtons[i]->setEnabled (true);
    }
}


void ChannelList::comboBoxChanged (ComboBox* b)
{

    //pulse waveform trigger selection.
    if (b == triggerSelectBox.get())
    {
        trig_source = b->getSelectedId();
    }

    //pulse or pattern stim.
    else if (b == outputTypeSelect.get())
    {
        selectedStimOutputType = (b->getSelectedId() == 2); //pulse out is 1, pattern out is 2
        board->setStimOutputType(selectedStimOutputType);

        //pulse stim
        if (!selectedStimOutputType)
        {
            patternOutputLabel->setVisible(false);
            //Initialize the led toggle buttons
            for (int i = 0; i < 64; ++i)
            {
                toggleButtons[i]->setEnabled(true);
            }
        }
        //pattern stim
        else
        {
            patternOutputLabel->setVisible (true);
            for (int i = 0; i < 64; ++i)
            {
                toggleButtons[i]->setEnabled (false);
                patternOutputLabel->setVisible (true);
            }
        }

    }
    else if (b == probeTypeSelect.get())
    {
        const int probeGeometry = b->getSelectedId();

        if (probeGeometry == 1) // 4-Shank Chronic
        {
            shankCount = 4;
            buttonCountPerShank = 16; 
            for (int i = 0; i < 64; i++)
            {
                ledMapping[i] = four_Shank_Chronic_mapping[i];
            }

        }
        else if (probeGeometry == 2) // 2-Shank Chronic
        {
            shankCount = 2;
            buttonCountPerShank = 32;
            for (int i = 0; i < 64; i++)
            {
                ledMapping[i] = two_Shank_Chronic_mapping[i];
            }
        }
        else if (probeGeometry == 3) // 4 Shank Acute
        {
            shankCount = 4;
            buttonCountPerShank = 16; 
            for (int i = 0; i < 64; i++)
            {
                ledMapping[i] = four_Shank_Acute_mapping[i];
            }
        }
        else if (probeGeometry == 4) // Demo Board (choose behavior)
        {
            shankCount = 4;
            buttonCountPerShank = 16;
            for (int i = 0; i < 64; i++)
            {
                ledMapping[i] = demo_Board_mapping[i];
            }
        }
         // Update LED layout/visibility
        update();
    }
}

void ChannelList::update()
{
    if (! board->foundInputSource())
    {
        disableAll();
        return;
    }

    staticLabels.clear();
    channelComponents.clear();
    uploadStimParamButton->setEnabled (true);

    // ---- FIXED LED GEOMETRY: always use 4-lane spacing ----
    const int ledsPerLane = 16; // fixed for 4-shank spacing
    const int lanesVisible = (shankCount == 1 ? 2 : 4);
    const int totalUsed = juce::jmin (64, lanesVisible * ledsPerLane);

    for (int i = 0; i < 64; ++i)
    {
        const bool used = (i < totalUsed);
        toggleButtons[i]->setVisible (used);
        toggleButtons[i]->setEnabled (used && ! selectedStimOutputType);
    }

    const float strokeW = 2.0f;
    const float gap = strokeW * 8.0f;

    const float startX = 50.0f;
    const float startY = 110.0f;

    const float shankW = 90.0f;
    const float shankH = 500.0f;

    const int ledSize = 20;

    const float topMargin = 25.0f;
    const float bottomMargin = 10.0f;

    const float yTop = startY + topMargin;
    const float yBottom = startY + shankH - bottomMargin;

    const float usable = juce::jmax (1.0f, yBottom - yTop);
    const float step = usable / (float) ledsPerLane;

    for (int lane = 0; lane < lanesVisible; ++lane)
    {
        const float x = startX + lane * (shankW + gap);
        const float cx = x + shankW * 0.5f;

        for (int j = 0; j < ledsPerLane; ++j)
        {
            const int index = lane * ledsPerLane + j;
            if (index >= 64)
                continue;

            // LED0 at tip/bottom for each lane
            const float cy = yBottom - (j + 0.5f) * step;

            toggleButtons[index]->setBounds ((int) std::round (cx - ledSize * 0.5f),
                                             (int) std::round (cy - ledSize * 0.5f),
                                             ledSize,
                                             ledSize);
        }
    }

    // keep LEDs clickable if anything overlaps
    for (int i = 0; i < 64; ++i)
        if (toggleButtons[i] && toggleButtons[i]->isVisible())
            toggleButtons[i]->toFront (false);

    repaint();
}

void ChannelList::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::grey);
    g.setColour (juce::Colours::black);

    const float strokeW = 2.0f;
    const float gap = strokeW * 8.0f;

    // Fixed geometry
    const float startX = 55.0f;
    const float startY = 130.0f;

    const float shankW = 90.0f;
    const float shankH = 500.0f;

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