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

#ifndef __DEVICEEDITOR_H_2AD3C591__
#define __DEVICEEDITOR_H_2AD3C591__

#include <VisualizerEditorHeaders.h>

namespace RhythmNode
{

class DeviceThread;
class ChannelCanvas;
class CableDelayInterface;
class SampleRateInterface;
 
struct ImpedanceData;

class DeviceEditor : public VisualizerEditor,
                     public ComboBox::Listener,
                     public Button::Listener
                     //public PopupChannelSelector::Listener

{
public:
    /** Constructor */
    DeviceEditor (GenericProcessor* parentNode, DeviceThread* thread);

    /** Destructor*/
    ~DeviceEditor() {}

    /** Respond to combo box changes (e.g. sample rate)*/
    void comboBoxChanged (ComboBox* comboBox) override;

    /** Respond to button clicks*/
    void buttonClicked (Button* button) override;

    /** Disable UI during acquisition*/
    void startAcquisition() override;

    /** Enable UI after acquisition is finished*/
    void stopAcquisition() override;

    /** Runs impedance test*/
    void measureImpedance();

    /** Callback when impedance measurement is finished */
    void impedanceMeasurementFinished();

    /** Saves impedance data to a file */
    void saveImpedance (File& file);

    /** Updates channel canvas*/
    void updateSettings() override;

    /** Saves custom parameters */
    void saveVisualizerEditorParameters (XmlElement* xml) override;

    /** Loads custom parameters*/
    void loadVisualizerEditorParameters (XmlElement* xml) override;

    /** Creates an interface with additional channel settings*/
    Visualizer* createNewCanvas (void) override;

   //** updates the cableDelay Combo box based calculated delay from sample rate.
    void updateCableDelayBox (int delay);

    /** Called by PopupChannelSelector */
   // void channelStateChanged (Array<int> newChannels) override;

    /** Called by PopupChannelSelector */
   // int getChannelCount() override;

   // virtual Array<int> getSelectedChannels() override { return Array<int>(); }

private:
   
    //sets cable delay of G3 Board
    std::unique_ptr<CableDelayInterface> cableDelayInterface;
    //Sets sample rate of G3 Board
    std::unique_ptr<SampleRateInterface> sampleRateInterface;
    //start and stop stim buttons
    std::unique_ptr<ColourButton> startStimButton;
    std::unique_ptr<ColourButton> stopStimButton;

    std::unique_ptr<Label> noBoardsDetectedLabel;

    // settings loaded from a saved signal chain while no board is connected,
    // written back unchanged on save so the configuration is not lost
    int savedSampleRateId = 0;
    int savedCableDelayId = 0;
    String savedSampleRateText;

    DeviceThread* board;
    ChannelCanvas* canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceEditor);
};


class SampleRateInterface : public Component,
                            public ComboBox::Listener
{
public:
    SampleRateInterface (DeviceThread*, DeviceEditor*);
    ~SampleRateInterface();

    int getSelectedId();
    void setSelectedId (int);

    String getText();

    void paint (Graphics& g);
    void comboBoxChanged (ComboBox* cb);

private:
    int sampleRate;
    String name;

    DeviceThread* board;
    DeviceEditor* editor;

    std::unique_ptr<ComboBox> rateSelection;
    StringArray sampleRateOptions;
};


	class CableDelayInterface : public Component,
                            public ComboBox::Listener
{
public:
    CableDelayInterface (DeviceThread*, DeviceEditor*);

    ~CableDelayInterface();

    int getSelectedId();
    void setSelectedId (int);

    void paint (Graphics& g);
    void comboBoxChanged (ComboBox* cb);

private:
    //int cableDelay;
    String name;

    DeviceThread* board;
    DeviceEditor* editor;

    ScopedPointer<ComboBox> delaySelection;
    StringArray cableDelayOptions;
};

} // namespace RhythmNode
#endif // __DEVICEEDITOR_H_2AD3C591__
