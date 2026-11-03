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

#ifndef __CHANNELLIST_H_2AD3C591__
#define __CHANNELLIST_H_2AD3C591__

#include <VisualizerEditorHeaders.h>

namespace RhythmNode
{

class DeviceThread;
class DeviceEditor;
class ChannelComponent;
class ChannelCanvas; 

class ChannelList : public Component,
                    public Button::Listener,
                    public ComboBox::Listener
{
public:
    /** Constructor */
    ChannelList (DeviceThread* board,
                 DeviceEditor* editor);

    /** Destructor */
    ~ChannelList() {}

    /** Updates label colors */
    void lookAndFeelChanged() override;

    /** Disables all channels */
    void disableAll();

    /** Enables all channels */
    void enableAll();

    /** Button callback */
    void buttonClicked (Button* btn) override;

    /** ComboBox callback */
    void comboBoxChanged (ComboBox* b) override;

    /** Updates layout of channel list */
    void update();

    /** Returns the maximum number of channels (used for setting layout) */
    int getMaxChannels() { return maxChannels; }

    /** Allows us to call repain() function from here depending on selected probe geometry*/
    void setOwnerCanvas (ChannelCanvas* canvas) { ownerCanvas = canvas; }

    void paint (juce::Graphics& g) override;


private:
    Array<float> gains;
    Array<ChannelInfoObject::Type> types;

    bool chainUpdate;

    DeviceThread* board;
    DeviceEditor* editor;

    //Acute Config only
    std::unique_ptr<UtilityButton> impedanceButton; //measures impedance.
    std::unique_ptr<UtilityButton> saveImpedanceButton; //saves impeadnce to file.

    //G3 Chip Stim Specific
    //galvani 3 waveform parameters
    int amplitude = 0, period = 0, pulse_width = 0, edge_sel = 0, pulse_count = 0;
    int trig_source = 1; //default is PC trigger.
    int repeatCount = 0;

    //Probe Geometry Selection (defaults to 4 shank)
    std::unique_ptr<ComboBox> probeTypeSelect;
    int shankCount = 4; // 4, 2, or 1
    int buttonCountPerShank = 16; // 16 for 4-shank, 32 for 2/1-shank

    //output type select
    std::unique_ptr<ComboBox> outputTypeSelect;
    std::unique_ptr<Label> outputTypeLabel;
    int selectedStimOutputType = 0; // 0=pulse, 1=pattern

    //upload stim param buttons
    std::unique_ptr<ColourButton> uploadStimParamButton;
    
    //pattern stim params
    std::unique_ptr<ColourButton> selectPatternFileButton;
    std::unique_ptr<TextEditor> patternRepeatCount;
    std::unique_ptr<Label> repeatLabel;

    //pulse stim params
    std::unique_ptr<Label> waveformInputLabel;
    std::unique_ptr<TextEditor> amplitudeInput;
    std::unique_ptr<TextEditor> periodInput;
    std::unique_ptr<TextEditor> pulseWidthInput;
    std::unique_ptr<ComboBox> edgeSelectBox;
    std::unique_ptr<TextEditor> pulseCountInput;
    std::unique_ptr<ComboBox> triggerSelectBox;


    int enabledLEDS[64];

    //galvani 3 LED Channel Toggle Buttons, Depends on the probe
    //geometry UPDATE THIS 
    std::unique_ptr<ToggleButton> toggleButtons[64];

   
    //displays if pattern output is selected. disables toggle button LEDS.
    std::unique_ptr<Label> patternOutputLabel;

    //**G3 LED Channel Mapping array
    //default
    int ledMapping[64] = {16, 31, 17, 30, 18, 29, 19, 28, 20, 27, 21, 26, 22, 25, 23, 24, 0, 15, 1,
        14, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8, 47, 32, 46, 33, 45, 34, 44, 35, 43, 36, 42, 37,
        41, 38, 40, 39, 63, 48, 62, 49, 61, 50, 60, 51, 59, 52, 58, 53, 57, 54, 56, 55 };

    //chronic 4 shank mapping.     
    int four_Shank_Chronic_mapping[64] = {16, 31, 17, 30, 18, 29, 19, 28, 20, 27, 21, 26, 22, 25, 23, 24, 0, 15, 1,
        14, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8, 47, 32, 46, 33, 45, 34, 44, 35, 43, 36, 42, 37,
        41, 38, 40, 39, 63, 48, 62, 49, 61, 50, 60, 51, 59, 52, 58, 53, 57, 54, 56, 55 };

    //chronic 2 shank mapping.  
    int two_Shank_Chronic_mapping[64] = {0, 31, 1, 30, 2, 29, 3, 28, 4, 27, 5, 26, 6, 25, 7, 24,
                     8, 23, 9, 22, 10, 21, 11, 20, 12, 19, 13, 18, 14, 17, 15, 16,
                     63, 32, 62, 33, 61, 34, 60, 35, 59, 36, 58, 37, 57, 38, 56, 39,
                     55, 40, 54, 41, 53, 42, 52, 43, 51, 44, 50, 45, 49, 46, 48, 47};
    
    //acute 4 shank mapping.  
    int four_Shank_Acute_mapping[64] = {48, 63, 49, 62, 50, 61, 51, 60, 52, 59, 53, 58, 54, 57, 55, 56,
                  47, 32, 46, 33, 45, 34, 44, 35, 43, 36, 42, 37, 41, 38, 40, 39,
                  0, 15, 1, 14, 2, 13, 3, 12, 4, 11, 5, 10, 6, 9, 7, 8,
                  31, 16, 30, 17, 29, 18, 28, 19, 27, 20, 26, 21, 25, 22, 24, 23};

    int demo_Board_mapping[64] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                    31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
                    63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
                    47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32};



    OwnedArray<Label> staticLabels;
    OwnedArray<ChannelComponent> channelComponents;

    int maxChannels;

    ChannelCanvas* ownerCanvas = nullptr;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelList);
};

} // namespace RhythmNode
#endif // __RHD2000EDITOR_H_2AD3C591__
