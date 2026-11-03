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

#include "DeviceEditor.h"
#include "DeviceThread.h"

#include "UI/ChannelCanvas.h"

#include <cmath>

using namespace RhythmNode;

#ifdef WIN32
#if (_MSC_VER < 1800) //round doesn't exist on MSVC prior to 2013 version
inline double round (double x)
{
    return floor (x + 0.5);
}
#endif
#endif

#define HS_WIDTH 70
#define HS_PANEL_WIDTH 80

DeviceEditor::DeviceEditor (GenericProcessor* parentNode,
                            DeviceThread* board_)
    : VisualizerEditor (parentNode, "HectoSTAR", 230), board (board_)
{
    canvas = nullptr;
    noBoardsDetectedLabel = nullptr;

    if (! board->foundInputSource())
    {
        noBoardsDetectedLabel = std::make_unique<Label> ("NoBoardsDetected", "No Recording Controller Detected.");
        noBoardsDetectedLabel->setBounds (0, 15, desiredWidth, 125);
        noBoardsDetectedLabel->setAlwaysOnTop (true);
        noBoardsDetectedLabel->toFront (false);
        noBoardsDetectedLabel->setJustificationType (Justification::centred);
        addAndMakeVisible (noBoardsDetectedLabel.get());

        return;
    }
      
    // add sample rate selection
    sampleRateInterface = std::make_unique<SampleRateInterface> (board, this);
    addAndMakeVisible (sampleRateInterface.get());
    sampleRateInterface->setBounds (10, 25, 80, 50);

    // add cable length selection (g3)
    cableDelayInterface = std::make_unique<CableDelayInterface> (board, this);
    addAndMakeVisible (cableDelayInterface.get());
    cableDelayInterface->setBounds (10, 65, 80, 50);

    //start stim
    startStimButton = std::make_unique<ColourButton> ("Start Stim", FontOptions (11.0f));
    startStimButton->setBounds (110, 35, 80, 25);
    startStimButton->setColours (Colours::black, Colours::green);
    startStimButton->addListener (this);
    addAndMakeVisible (startStimButton.get());

    //stop stim
    stopStimButton = std::make_unique<ColourButton> ("Stop Stim", FontOptions (11.0f));
    stopStimButton->setBounds (110, 75, 80, 25);
    stopStimButton->setColours (Colours::black, Colours::darkred);
    stopStimButton->addListener (this);
    addAndMakeVisible (stopStimButton.get());
}


//updates whenever sample rate is changed
void DeviceEditor::updateCableDelayBox (int delay)
{
    LOGD ("Cable delay set to ", delay);
    cableDelayInterface->setSelectedId (delay);
}

void DeviceEditor::measureImpedance()
{
    board->runImpedanceTest();

    CoreServices::updateSignalChain (this);
}

void DeviceEditor::saveImpedance (File& file)
{
    LOGD ("Saving impedances to ", file.getFullPathName());

    board->saveImpedances (file);
}

void DeviceEditor::updateSettings()
{
    if (canvas != nullptr)
    {
        canvas->update();
    }
}

void DeviceEditor::comboBoxChanged (ComboBox* comboBox)
{
}

void DeviceEditor::buttonClicked (Button* button)
{
    LOGD ("Button Clicked");
    if (button == startStimButton.get())
    {
        board->startStim();
    }
    else if (button == stopStimButton.get())
    {
        board->stopStim();
    }
}

void DeviceEditor::startAcquisition()
{
    acquisitionIsActive = true;
}

void DeviceEditor::stopAcquisition()
{
    acquisitionIsActive = false;
}

//TODO, save the waveform parametrs XML
void DeviceEditor::saveVisualizerEditorParameters (XmlElement* xml)
{
    xml->setAttribute ("CableDelay", cableDelayInterface->getSelectedId());
    xml->setAttribute ("SampleRate", sampleRateInterface->getSelectedId());
    xml->setAttribute ("SampleRateString", sampleRateInterface->getText());
}

void DeviceEditor::loadVisualizerEditorParameters (XmlElement* xml)
{
    sampleRateInterface->setSelectedId (xml->getIntAttribute ("SampleRate"));
    cableDelayInterface->setSelectedId (xml->getIntAttribute ("CableDelay"));
}

Visualizer* DeviceEditor::createNewCanvas()
{
    GenericProcessor* processor = (GenericProcessor*) getProcessor();

    canvas = new ChannelCanvas (board, this);

    return canvas;
}

// Sample rate Options --------------------------------------------------------------------

SampleRateInterface::SampleRateInterface (DeviceThread* board_,
                                          DeviceEditor* editor_) : board (board_), editor (editor_)
{
    name = "Sample Rate";

    sampleRateOptions.add ("1.00 kS/s");
    sampleRateOptions.add ("1.25 kS/s");
    sampleRateOptions.add ("1.50 kS/s");
    sampleRateOptions.add ("2.00 kS/s");
    sampleRateOptions.add ("2.50 kS/s");
    sampleRateOptions.add ("3.00 kS/s");
    sampleRateOptions.add ("3.33 kS/s");
    sampleRateOptions.add ("4.00 kS/s");
    sampleRateOptions.add ("5.00 kS/s");
    sampleRateOptions.add ("6.25 kS/s");
    sampleRateOptions.add ("8.00 kS/s");
    sampleRateOptions.add ("10.0 kS/s");
    sampleRateOptions.add ("12.5 kS/s");
    sampleRateOptions.add ("15.0 kS/s");
    sampleRateOptions.add ("20.0 kS/s");
    sampleRateOptions.add ("25.0 kS/s");
    sampleRateOptions.add ("30.0 kS/s");

    rateSelection = std::make_unique<ComboBox>("Sample Rate");
    rateSelection->addItemList (sampleRateOptions, 1);
    rateSelection->setSelectedId (15, dontSendNotification);
    rateSelection->addListener (this);
    rateSelection->setBounds (0, 12, 80, 20);
    addAndMakeVisible(rateSelection.get());

    LOGD ("DEFAULT SAMPLE RATE SET TO 20000");
}

SampleRateInterface::~SampleRateInterface()
{
}

void SampleRateInterface::comboBoxChanged (ComboBox* cb)
{
    if (! (editor->acquisitionIsActive) && board->foundInputSource())
    {
        if (cb == rateSelection.get())
        {
            int delay = board->setSampleRate(cb->getSelectedId());

            LOGD ("Setting sample rate to ", cb->getText());
            LOGD ("Updating cable delay box to reflect new sample rate")

            editor->updateCableDelayBox (delay);
            CoreServices::updateSignalChain (editor);
        }
    }
}

int SampleRateInterface::getSelectedId()
{
    return rateSelection->getSelectedId();
}

void SampleRateInterface::setSelectedId (int id)
{
    rateSelection->setSelectedId (id);
}

String SampleRateInterface::getText()
{
    return rateSelection->getText();
}

void SampleRateInterface::paint (Graphics& g)
{
    g.setColour (Colours::darkgrey);

    g.setFont (Font ("Small Text", 10, Font::plain));

    g.drawText (name, 0, 0, 80, 15, Justification::left, false);
}

// Cable Delay Options --------------------------------------------------------------------
CableDelayInterface::CableDelayInterface (DeviceThread* board_,
                                          DeviceEditor* editor_) : board (board_), editor (editor_)
{
    name = "Cable Delay";

    cableDelayOptions.add ("1");
    cableDelayOptions.add ("2");
    cableDelayOptions.add ("3");
    cableDelayOptions.add ("4");
    cableDelayOptions.add ("5");
    cableDelayOptions.add ("6");
    cableDelayOptions.add ("7");
    cableDelayOptions.add ("8");
    cableDelayOptions.add ("9");
    cableDelayOptions.add ("10");
    cableDelayOptions.add ("11");
    cableDelayOptions.add ("12");
    cableDelayOptions.add ("13");
    cableDelayOptions.add ("14");
    cableDelayOptions.add ("15");

    delaySelection = new ComboBox ("Cable Delay");
    delaySelection->addItemList (cableDelayOptions, 1);
    delaySelection->setSelectedId (5, dontSendNotification); //default to cable delay 5
    delaySelection->setBounds (0, 12, 80, 20);
    delaySelection->addListener (this);
    addAndMakeVisible (delaySelection);

    //set board delay (port C (#2) is G3)
    LOGD ("Default Cable Delay set to 5");
    board->setCableDelay (2, delaySelection->getSelectedId());
}

CableDelayInterface::~CableDelayInterface()
{
}

int CableDelayInterface::getSelectedId()
{
    return delaySelection->getSelectedId();
}

void CableDelayInterface::setSelectedId (int id)
{
    delaySelection->setSelectedId (id);
}

void CableDelayInterface::paint (Graphics& g)
{
    g.setColour (Colours::darkgrey);

    g.setFont (Font ("Small Text", 10, Font::plain));

    g.drawText (name, 0, 0, 100, 15, Justification::left, false);
}

void CableDelayInterface::comboBoxChanged (ComboBox* cb)
{
    if (! (editor->acquisitionIsActive) && board->foundInputSource())
    {
        if (cb == delaySelection)
        {
            LOGD ("Setting cable delay to index ", cb->getSelectedId());

            board->setCableDelay (2, cb->getSelectedId()); //fixed for port C (G3)

            CoreServices::updateSignalChain (editor);
        }
    }
}

/*
void DeviceEditor::measureImpedance()
{
    if (! acquisitionIsActive)
    {
        board->runImpedanceTest();
    }
}

void DeviceEditor::impedanceMeasurementFinished()
{
    if (canvas != nullptr)
    {
        canvas->updateAsync();
    }
}

void DeviceEditor::saveImpedance (File& file)
{
    LOGD ("Saving impedances to ", file.getFullPathName());

    board->saveImpedances (file);
}
*/
