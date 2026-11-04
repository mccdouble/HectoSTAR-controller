/*
    ------------------------------------------------------------------

    Pattern stim mode panel. See PatternPanel.h.

    ------------------------------------------------------------------
*/

#include "PatternPanel.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

String formatSeconds (double s)
{
    String t (s, 2);
    if (t.containsChar ('.'))
        t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return t + " s";
}

} // namespace

//==============================================================================

TimelinePane::TimelinePane (PatternDocument& d, Playback& p) : doc (d), playback (p), timeline (d)
{
    title.setFont (FontOptions (14.0f, Font::bold));
    addAndMakeVisible (title);

    playButton.setTooltip ("Play the pattern on screen: the LEDs on the probe light up with their current (nothing is sent to the board)");
    playButton.onClick = [this]
    {
        if (playback.isPlaying())
            playback.pause();
        else
            playback.play();
    };
    stopButton.onClick = [this] { playback.stop(); };

    speedBox.addItem ("Real time", 1);
    speedBox.addItem ("1/2 speed", 2);
    speedBox.addItem ("1/10 speed", 3);
    speedBox.addItem ("1/100 speed", 4);
    speedBox.setSelectedId (1, dontSendNotification);
    speedBox.setTooltip ("Slow the playback down to see short pulses and their shapes");
    speedBox.onChange = [this]
    {
        static const double speeds[] = { 1.0, 0.5, 0.1, 0.01 };
        playback.setSpeed (speeds[jlimit (1, 4, speedBox.getSelectedId()) - 1]);
    };

    playTime.setFont (FontOptions (13.0f));

    dockButton.setTooltip ("Move the timeline to its own window, or back into the stim panel");
    dockButton.onClick = [this]
    {
        if (onDockToggle)
            onDockToggle();
    };

    for (Component* c : { (Component*) &playButton, (Component*) &stopButton, (Component*) &speedBox, (Component*) &playTime, (Component*) &dockButton })
        addAndMakeVisible (c);

    timeline.setPlayback (&playback);
    timeline.onLaneNameClicked = [this] (int led)
    {
        // click: just this LED; Ctrl/Cmd-click: add or remove it
        auto sel = doc.selectedLeds();
        if (ModifierKeys::currentModifiers.isCommandDown())
        {
            if (sel.count (led) > 0)
                sel.erase (led);
            else
                sel.insert (led);
        }
        else
            sel = { led };
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds (sel);
    };

    inUseButton.setTooltip ("Select every LED that has a waveform");
    inUseButton.onClick = [this]
    {
        const auto used = activeLeds (doc.pattern());
        doc.setSelectedBlocks ({});
        doc.setSelectedLeds (std::set<int> (used.begin(), used.end()));
    };
    addAndMakeVisible (inUseButton);
    addAndMakeVisible (timeline);

    playback.addChangeListener (this);
    doc.addChangeListener (this);
    updatePlayControls();
}

TimelinePane::~TimelinePane()
{
    doc.removeChangeListener (this);
    playback.removeChangeListener (this);
    timeline.setPlayback (nullptr);
}

void TimelinePane::setDocked (bool docked)
{
    dockButton.setLabel (docked ? "Pop out" : "Dock");
    dockButton.repaint();
}

void TimelinePane::updatePlayControls()
{
    playButton.setLabel (playback.isPlaying() ? "Pause" : "Play");
    playButton.setStyle (playback.isPlaying() ? EditorButton::Style::Accent : EditorButton::Style::Normal);
    stopButton.setActive (playback.isActive());

    String t;
    if (playback.isActive())
    {
        t << String (playback.getTime(), 3) << " s";
        if (doc.pattern().repeatCount > 1)
            t << "   run " << playback.getRun() << " of " << doc.pattern().repeatCount;
    }
    playTime.setText (t, dontSendNotification);
}

void TimelinePane::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
    g.setColour (findColour (ThemeColours::widgetBackground));
    g.fillRect (0, 0, getWidth(), kToolbarHeight);
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (kToolbarHeight - 1, 0.0f, (float) getWidth());
}

void TimelinePane::resized()
{
    auto b = getLocalBounds();
    auto bar = b.removeFromTop (kToolbarHeight).reduced (10, 4);

    dockButton.setBounds (bar.removeFromRight (80));
    bar.removeFromRight (6);
    inUseButton.setBounds (bar.removeFromRight (150));
    title.setBounds (bar.removeFromLeft (80));
    playButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (4);
    stopButton.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (8);
    speedBox.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (10);
    playTime.setBounds (bar);

    timeline.setBounds (b);
}

//==============================================================================

/** The bar above the docked timeline: drag it to resize the timeline. */
class PatternPanel::Divider : public Component,
                              public SettableTooltipClient
{
public:
    explicit Divider (PatternPanel& o) : owner (o)
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setTooltip ("Drag to resize the timeline");
    }

    void paint (Graphics& g) override
    {
        g.setColour (findColour (ThemeColours::outline));
        const float y = getHeight() * 0.5f;
        g.drawHorizontalLine ((int) y, 0.0f, (float) getWidth());
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse (getWidth() * 0.5f + i * 8.0f - 1.5f, y - 1.5f, 3.0f, 3.0f);
    }

    void mouseDown (const MouseEvent&) override { startHeight = owner.timelineHeight; }

    void mouseDrag (const MouseEvent& e) override
    {
        owner.timelineHeight = startHeight - e.getDistanceFromDragStartY();
        owner.resized();
        owner.repaint();
    }

private:
    PatternPanel& owner;
    int startHeight = 0;
};

/** The timeline in a window of its own. Closing the window docks it again. */
class PatternPanel::TimelineWindow : public DocumentWindow
{
public:
    TimelineWindow (PatternPanel& o, TimelinePane& pane)
        : DocumentWindow ("HectoSTAR pattern timeline", o.findColour (ThemeColours::componentBackground), DocumentWindow::allButtons),
          owner (o)
    {
        setLookAndFeel (&owner.getLookAndFeel());
        setContentNonOwned (&pane, false);
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (500, 200, 10000, 10000);

        auto area = owner.getScreenBounds();
        setBounds (area.getX(), area.getBottom() - 380, jmax (900, area.getWidth()), 380);
    }

    ~TimelineWindow() override
    {
        clearContentComponent();
        setLookAndFeel (nullptr);
    }

    void closeButtonPressed() override
    {
        Component::SafePointer<PatternPanel> safeOwner (&owner);
        MessageManager::callAsync ([safeOwner]
                                   {
                                       if (safeOwner != nullptr)
                                           safeOwner->setTimelineDocked (true);
                                   });
    }

private:
    PatternPanel& owner;
};

PatternPanel::PatternPanel (PatternDocument& d) : doc (d), playback (d), designer (d), timelinePane (d, playback)
{
    timelinePane.onDockToggle = [this]
    {
        // async: the click may come from inside the window that docking deletes
        Component::SafePointer<PatternPanel> safeThis (this);
        const bool dock = ! timelineDocked;
        MessageManager::callAsync ([safeThis, dock]
                                   {
                                       if (safeThis != nullptr)
                                           safeThis->setTimelineDocked (dock);
                                   });
    };
    addAndMakeVisible (timelinePane);
    divider = std::make_unique<Divider> (*this);
    addAndMakeVisible (divider.get());

    setWantsKeyboardFocus (true);

    fileButton.setTooltip ("New, open or save a pattern design (.json), load or export a pattern file (.bin)");
    fileButton.onClick = [this] { showFileMenu(); };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    undoButton.onClick = [this] { doc.undo(); };
    redoButton.onClick = [this] { doc.redo(); };
    for (auto* b : { &fileButton, &undoButton, &redoButton })
        addAndMakeVisible (b);

    for (auto* l : { &lengthLabel, &repeatLabel, &usageLabel, &boardLabel })
    {
        l->setFont (FontOptions (13.0f));
        addAndMakeVisible (l);
    }
    lengthLabel.setJustificationType (Justification::centredRight);
    repeatLabel.setJustificationType (Justification::centredRight);
    boardLabel.setJustificationType (Justification::centredRight);

    lengthEditor.setInputRestrictions (10, "0123456789.");
    lengthEditor.setJustification (Justification::centredRight);
    lengthEditor.setTooltip ("Length of one run of the pattern. The board holds up to 13.1 s.");
    lengthEditor.onReturnKey = lengthEditor.onFocusLost = [this] { commitLength(); };
    repeatEditor.setInputRestrictions (6, "0123456789");
    repeatEditor.setJustification (Justification::centredRight);
    repeatEditor.setTooltip ("How many times the board plays the pattern each time stim starts");
    repeatEditor.onReturnKey = repeatEditor.onFocusLost = [this] { commitRepeats(); };
    addAndMakeVisible (lengthEditor);
    addAndMakeVisible (repeatEditor);

    addAndMakeVisible (designer);

    doc.addChangeListener (this);
    refresh();
}

PatternPanel::~PatternPanel()
{
    playback.stop();
    doc.removeChangeListener (this);
    if (timelineWindow != nullptr)
        timelineWindow->clearContentComponent();
    timelineWindow.reset();
}

void PatternPanel::setTimelineDocked (bool docked)
{
    if (docked == timelineDocked)
        return;
    timelineDocked = docked;

    if (docked)
    {
        if (timelineWindow != nullptr)
            timelineWindow->clearContentComponent();
        timelineWindow.reset();
        addAndMakeVisible (timelinePane);
        divider->setVisible (true);
    }
    else
    {
        removeChildComponent (&timelinePane);
        divider->setVisible (false);
        timelineWindow = std::make_unique<TimelineWindow> (*this, timelinePane);
        timelineWindow->setVisible (true);
        timelineWindow->toFront (true);
    }

    timelinePane.setDocked (docked);
    resized();
    repaint();
}

void PatternPanel::changeListenerCallback (ChangeBroadcaster*)
{
    // blocks picked on the timeline select their LEDs (so the designer loads them)
    if (doc.selectedBlocks() != lastBlockSelection)
    {
        lastBlockSelection = doc.selectedBlocks();
        if (! lastBlockSelection.empty())
        {
            std::set<int> leds;
            for (int id : lastBlockSelection)
                if (const Block* b = doc.findBlock (id))
                    leds.insert (b->led);
            if (leds != doc.selectedLeds())
                doc.setSelectedLeds (leds);
        }
    }

    refresh();
}

void PatternPanel::refresh()
{
    const Pattern& p = doc.pattern();

    if (! lengthEditor.hasKeyboardFocus (true))
        lengthEditor.setText (String (p.duration, 3).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."), false);
    if (! repeatEditor.hasKeyboardFocus (true))
        repeatEditor.setText (String (p.repeatCount), false);

    const double maxSeconds = (double) kMaxSamples / kSampleRateHz;
    String usage;
    usage << "Board memory: " << formatSeconds (p.duration) << " of " << formatSeconds (maxSeconds)
          << "  (plays " << formatSeconds (p.duration * p.repeatCount) << ")";
    usageLabel.setText (usage, dontSendNotification);
    usageLabel.setColour (Label::textColourId, fitsInFlash (p) ? findColour (ThemeColours::defaultText) : Colours::red);

    undoButton.setActive (doc.canUndo());
    redoButton.setActive (doc.canRedo());

    resized();
}

void PatternPanel::setBoardStatus (const String& text, bool upToDate)
{
    boardLabel.setText (text, dontSendNotification);
    boardLabel.setColour (Label::textColourId, upToDate ? findColour (ThemeColours::defaultText) : Colour (0xffd35400));
}

void PatternPanel::commitLength()
{
    const double v = lengthEditor.getText().getDoubleValue();
    if (v <= 0 || v == doc.pattern().duration)
    {
        refresh();
        return;
    }
    doc.modify ([v] (Pattern& p) { p.duration = v; });
}

void PatternPanel::commitRepeats()
{
    const int v = jmax (1, repeatEditor.getText().getIntValue());
    if (v == doc.pattern().repeatCount)
    {
        refresh();
        return;
    }
    doc.setRepeatCount (v);
}

void PatternPanel::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (headerBottom, 0.0f, (float) getWidth());
}

void PatternPanel::resized()
{
    auto b = getLocalBounds().reduced (10, 8);

    auto header = b.removeFromTop (26);
    fileButton.setBounds (header.removeFromLeft (64));
    header.removeFromLeft (6);
    undoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    redoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (4);
    header.removeFromLeft (14);
    lengthLabel.setBounds (header.removeFromLeft (textWidth (lengthLabel.getText()) + 10));
    lengthEditor.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (6);
    repeatLabel.setBounds (header.removeFromLeft (textWidth (repeatLabel.getText()) + 10));
    repeatEditor.setBounds (header.removeFromLeft (50));
    header.removeFromLeft (8);
    boardLabel.setBounds (header.removeFromRight (jmin (300, jmax (0, header.getWidth() - 200))));
    usageLabel.setBounds (header);

    headerBottom = b.getY() + 4;
    b.removeFromTop (12);

    if (timelineDocked)
    {
        // keep at least 260 px for the waveform settings above it
        timelineHeight = jlimit (140, jmax (140, b.getHeight() - 260), timelineHeight);
        timelinePane.setBounds (b.removeFromBottom (timelineHeight).expanded (10, 0).withTrimmedBottom (-8));
        divider->setBounds (b.removeFromBottom (10).expanded (10, 0));
        b.removeFromBottom (4);
    }

    designer.setBounds (b);
}

bool PatternPanel::keyPressed (const KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (! mods.isCommandDown())
        return false;
    const int c = key.getKeyCode();
    if (c == 'Z' || c == 'z')
    {
        if (mods.isShiftDown())
            doc.redo();
        else
            doc.undo();
        return true;
    }
    if (c == 'Y' || c == 'y')
    {
        doc.redo();
        return true;
    }
    if (c == 'S' || c == 's')
    {
        savePattern (mods.isShiftDown());
        return true;
    }
    return false;
}

void PatternPanel::showFileMenu()
{
    PopupMenu menu;
    menu.addItem (1, "New pattern");
    menu.addItem (2, "Open design (.json)...");
    menu.addSeparator();
    menu.addItem (3, "Save design" + String (doc.file != File() ? " (" + doc.file.getFileName() + ")" : String()));
    menu.addItem (4, "Save design as...");
    menu.addSeparator();
    menu.addItem (6, "Load a pattern file (.bin)...");
    menu.addItem (5, "Export pattern file (.bin)...");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&fileButton),
                        [this] (int result)
                        {
                            switch (result)
                            {
                                case 1: newPattern(); break;
                                case 2: openPattern(); break;
                                case 3: savePattern (false); break;
                                case 4: savePattern (true); break;
                                case 5: exportBin(); break;
                                case 6: loadBin(); break;
                                default: break;
                            }
                        });
}

void PatternPanel::confirmDiscard (std::function<void()> then)
{
    if (! doc.dirty || doc.pattern().blocks.empty())
    {
        then();
        return;
    }

    AlertWindow::showOkCancelBox (AlertWindow::QuestionIcon, "Pattern stim",
                                  "Discard the changes to the current pattern?",
                                  "Discard", "Cancel", this,
                                  ModalCallbackFunction::create ([then] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         then();
                                                                 }));
}

void PatternPanel::newPattern()
{
    confirmDiscard ([this]
                    {
                        Pattern p;
                        p.probe = doc.pattern().probe;
                        p.repeatCount = doc.pattern().repeatCount;
                        doc.reset (p, File());
                        if (onPatternReplaced)
                            onPatternReplaced();
                    });
}

void PatternPanel::openPattern()
{
    confirmDiscard ([this]
                    {
                        chooser = std::make_unique<FileChooser> ("Open pattern design", doc.file, "*.json");
                        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                              [this] (const FileChooser& fc)
                                              {
                                                  const File f = fc.getResult();
                                                  if (! f.existsAsFile())
                                                      return;

                                                  Pattern p;
                                                  std::string error;
                                                  if (! fromJson (f.loadFileAsString().toStdString(), p, error))
                                                  {
                                                      AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Open pattern", String (error));
                                                      return;
                                                  }
                                                  doc.reset (p, f);
                                                  if (onPatternReplaced)
                                                      onPatternReplaced();
                                              });
                    });
}

void PatternPanel::savePattern (bool askForFile)
{
    auto write = [this] (File f)
    {
        if (! f.hasFileExtension ("json"))
            f = f.withFileExtension ("json");
        if (! f.replaceWithText (String::fromUTF8 (toJson (doc.pattern()).c_str())))
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Save pattern", "Couldn't write " + f.getFullPathName());
            return;
        }
        doc.file = f;
        doc.dirty = false;
        refresh();
    };

    if (! askForFile && doc.file != File())
    {
        write (doc.file);
        return;
    }

    chooser = std::make_unique<FileChooser> ("Save pattern design",
                                             doc.file == File() ? File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("pattern.json") : doc.file,
                                             "*.json");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [write] (const FileChooser& fc)
                          {
                              if (fc.getResult() != File())
                                  write (fc.getResult());
                          });
}

void PatternPanel::prepareOutput (const String& action, std::function<void (std::vector<std::uint8_t>)> onReady)
{
    const Pattern& p = doc.pattern();

    if (p.blocks.empty())
    {
        AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, action, "The pattern is empty: select LEDs on the probe and apply a waveform first.");
        return;
    }

    const auto problems = validate (p);
    if (! problems.empty())
    {
        String msg;
        for (const auto& s : problems)
            msg << "- " << String::fromUTF8 (s.c_str()).replace ("uA", kMicroAmps) << "\n";
        AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, action, "Fix these first:\n\n" + msg);
        return;
    }

    auto generate = [this, action, onReady]
    {
        PatternData data;
        std::string error;
        if (! renderPattern (doc.pattern(), data, error))
        {
            AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, action, String (error));
            return;
        }
        onReady (encodePatternFile (data, doc.pattern().probe));
    };

    if (isProbeMappingVerified (p.probe))
    {
        generate();
        return;
    }

    AlertWindow::showOkCancelBox (AlertWindow::WarningIcon, action,
                                  String ("The ") + probeMappingName (p.probe) + " LED mapping hasn't been verified on a device yet. Continue?",
                                  "Continue", "Cancel", this,
                                  ModalCallbackFunction::create ([generate] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         generate();
                                                                 }));
}

void PatternPanel::exportBin()
{
    prepareOutput ("Export pattern file", [this] (std::vector<std::uint8_t> bytes)
                   {
                       const File start = doc.file != File() ? doc.file.withFileExtension ("bin")
                                                             : File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("DACData.bin");
                       chooser = std::make_unique<FileChooser> ("Export pattern file", start, "*.bin");
                       chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                                             [bytes] (const FileChooser& fc)
                                             {
                                                 File f = fc.getResult();
                                                 if (f == File())
                                                     return;
                                                 if (! f.hasFileExtension ("bin"))
                                                     f = f.withFileExtension ("bin");
                                                 f.deleteFile();
                                                 if (! f.appendData (bytes.data(), bytes.size()))
                                                     AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Export pattern file", "Couldn't write " + f.getFullPathName());
                                             });
                   });
}

void PatternPanel::loadBin()
{
    confirmDiscard ([this]
                    {
                        chooser = std::make_unique<FileChooser> ("Load a pattern file", File::getSpecialLocation (File::userDocumentsDirectory), "*.bin");
                        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                              [this] (const FileChooser& fc)
                                              {
                                                  const File f = fc.getResult();
                                                  if (! f.existsAsFile())
                                                      return;

                                                  MemoryBlock data;
                                                  if (! f.loadFileAsData (data))
                                                  {
                                                      AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Load pattern file", "Couldn't read " + f.getFullPathName());
                                                      return;
                                                  }

                                                  const auto* bytes = static_cast<const std::uint8_t*> (data.getData());
                                                  Pattern p;
                                                  std::string error;
                                                  if (! decodePatternFile (std::vector<std::uint8_t> (bytes, bytes + data.getSize()), doc.pattern().probe, p, error))
                                                  {
                                                      AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Load pattern file", String (error));
                                                      return;
                                                  }

                                                  for (auto& b : p.blocks)
                                                      b.label = f.getFileNameWithoutExtension().toStdString();
                                                  p.repeatCount = doc.pattern().repeatCount;
                                                  doc.reset (p, File());
                                                  if (onPatternReplaced)
                                                      onPatternReplaced();

                                                  if (p.blocks.empty())
                                                      AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, "Load pattern file", f.getFileName() + " has no current on any LED.");
                                              });
                    });
}

} // namespace PatternEditor
