/*
    ------------------------------------------------------------------

    HectoSTAR pattern editor window. See PatternEditorWindow.h.

    ------------------------------------------------------------------
*/

#include "PatternEditorWindow.h"

#include <cmath>

namespace PatternEditor
{

namespace
{

constexpr ProbeMapping kProbes[] = { ProbeMapping::FourShankChronic, ProbeMapping::TwoShankChronic,
                                     ProbeMapping::FourShankAcute, ProbeMapping::DemoBoard };

String probeItemName (ProbeMapping m)
{
    String name (probeMappingName (m));
    if (! isProbeMappingVerified (m))
        name << " (unverified)";
    return name;
}

String formatSeconds (double s)
{
    return String (s, s < 10 ? 3 : 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " s";
}

} // namespace

PatternEditorComponent::PatternEditorComponent (ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload)
    : uploadCallback (std::move (onUpload)),
      probePanel (doc),
      addBar (doc),
      timeline (doc),
      inspector (doc)
{
    setWantsKeyboardFocus (true);

    Pattern p;
    p.probe = initialProbe;
    doc.reset (p, File());

    for (auto* b : { &fileButton, &undoButton, &redoButton, &uploadButton })
        addAndMakeVisible (b);

    fileButton.onClick = [this] { showFileMenu(); };
    fileButton.setTooltip ("New, open, save the design (.json), export the pattern file (.bin)");
    undoButton.onClick = [this] { doc.undo(); };
    redoButton.onClick = [this] { doc.redo(); };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    uploadButton.onClick = [this] { upload(); };
    uploadButton.setTooltip ("Generate the pattern file and write it to the board's flash (Pattern Output is selected afterwards)");

    for (auto* l : { &durationLabel, &probeLabel, &repeatLabel })
    {
        l->setFont (FontOptions (13.0f));
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }

    durationEditor.setInputRestrictions (10, "0123456789.");
    durationEditor.setJustification (Justification::centredRight);
    durationEditor.onReturnKey = durationEditor.onFocusLost = [this]
    {
        const double v = durationEditor.getText().getDoubleValue();
        if (v > 0 && v != doc.pattern().duration)
        {
            doc.modify ([v] (Pattern& p) { p.duration = v; });
            timeline.zoomToFit();
        }
        else
        {
            refresh();
        }
    };
    addAndMakeVisible (durationEditor);

    repeatEditor.setInputRestrictions (6, "0123456789");
    repeatEditor.setJustification (Justification::centredRight);
    repeatEditor.setTooltip ("How many times the board plays the pattern");
    repeatEditor.onReturnKey = repeatEditor.onFocusLost = [this]
    {
        const int v = repeatEditor.getText().getIntValue();
        if (v >= 1 && v != doc.pattern().repeatCount)
            doc.modify ([v] (Pattern& p) { p.repeatCount = v; });
        else
            refresh();
    };
    addAndMakeVisible (repeatEditor);

    for (auto m : kProbes)
        probeBox.addItem (probeItemName (m), (int) m);
    probeBox.onChange = [this]
    {
        const auto m = (ProbeMapping) probeBox.getSelectedId();
        if (m != doc.pattern().probe)
            doc.modify ([m] (Pattern& p) { p.probe = m; });
    };
    addAndMakeVisible (probeBox);

    usageLabel.setFont (FontOptions (13.0f));
    addAndMakeVisible (usageLabel);

    addAndMakeVisible (probePanel);
    addAndMakeVisible (addBar);
    addAndMakeVisible (timeline);
    addAndMakeVisible (inspector);
    inspector.onLayoutChanged = [this] { resized(); };

    statusLabel.setFont (FontOptions (13.0f));
    statusLabel.setTooltip ("Probe map: click / Ctrl-click / drag to select LEDs.  Timeline: click a block to select it (with its linked copies; Alt for just one), "
                            "Shift or Ctrl to add; drag to move, drag an edge to resize (snaps to the ruler; hold Alt for single 50 us samples); "
                            "double-click a lane to add the Add bar's waveform; double-click the ruler to fit; Ctrl+wheel zooms, Shift+wheel scrolls; "
                            "Delete removes; Ctrl+A selects all; Ctrl+Z / Ctrl+Y undo / redo; Ctrl+S saves.");
    addAndMakeVisible (statusLabel);

    timeline.onLaneNameClicked = [this] (int led)
    {
        auto sel = doc.selectedLeds();
        sel.count (led) > 0 ? (void) sel.erase (led) : (void) sel.insert (led);
        doc.setSelectedLeds (sel);
    };

    doc.addChangeListener (this);
    refresh();
}

PatternEditorComponent::~PatternEditorComponent()
{
    doc.removeChangeListener (this);
}

void PatternEditorComponent::changeListenerCallback (ChangeBroadcaster*)
{
    refresh();
}

String PatternEditorComponent::documentTitle() const
{
    String name = doc.file == File() ? String ("Untitled pattern") : doc.file.getFileName();
    if (doc.dirty)
        name << " *";
    return name;
}

void PatternEditorComponent::refresh()
{
    const Pattern& p = doc.pattern();

    if (! durationEditor.hasKeyboardFocus (true))
        durationEditor.setText (String (p.duration), false);
    if (! repeatEditor.hasKeyboardFocus (true))
        repeatEditor.setText (String (p.repeatCount), false);
    probeBox.setSelectedId ((int) p.probe, dontSendNotification);

    undoButton.setActive (doc.canUndo());
    redoButton.setActive (doc.canRedo());

    const double maxSeconds = (double) kMaxSamples / kSampleRateHz;
    const double mb = encodedSizeBytes (p) / (1024.0 * 1024.0);
    String usage;
    usage << "Flash: " << formatSeconds (p.duration) << " of " << formatSeconds (maxSeconds) << " (" << String (mb, 1) << " of 16 MB)";
    if (p.repeatCount > 1)
        usage << ", plays " << p.repeatCount << "x = " << formatSeconds (p.duration * p.repeatCount);
    usageLabel.setText (usage, dontSendNotification);
    usageLabel.setColour (Label::textColourId, fitsInFlash (p) ? findColour (ThemeColours::defaultText) : Colours::red);

    const auto problems = validate (p);
    if (! problems.empty())
    {
        statusLabel.setColour (Label::textColourId, Colours::red);
        statusLabel.setText (String (problems.front()) + (problems.size() > 1 ? "  (+" + String ((int) problems.size() - 1) + " more)" : String()),
                             dontSendNotification);
    }
    else
    {
        String hint;
        if (p.blocks.empty())
            hint = "Select LEDs on the probe map, pick a waveform in the Add bar, then click Add.";
        else if (doc.selectedBlocks().empty())
            hint = "Click a block to edit it. Drag to move, drag an edge to resize, Delete removes.";
        else
            hint = "Editing the selected blocks below. Drag to move them; Alt-drag moves by single samples.";

        statusLabel.setColour (Label::textColourId, findColour (ThemeColours::defaultText).withAlpha (0.75f));
        statusLabel.setText (String ((int) p.blocks.size()) + " blocks on " + String ((int) activeLeds (p).size()) + " LEDs.   " + hint
                                 + "   (hover here for all shortcuts)",
                             dontSendNotification);
    }

    if (onTitleChanged)
        onTitleChanged();
}

void PatternEditorComponent::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));

    g.setColour (findColour (ThemeColours::widgetBackground));
    g.fillRect (0, 0, getWidth(), kToolbarHeight);

    g.setColour (findColour (ThemeColours::outline));
    g.drawHorizontalLine (kToolbarHeight, 0.0f, (float) getWidth());
    g.drawVerticalLine (probePanel.getRight(), (float) probePanel.getY(), (float) probePanel.getBottom());
    g.drawHorizontalLine (inspector.getY(), (float) inspector.getX(), (float) getWidth());
    g.drawHorizontalLine (statusLabel.getY() - 1, 0.0f, (float) getWidth());
}

void PatternEditorComponent::resized()
{
    auto b = getLocalBounds();

    // one top row: file and undo, pattern settings, flash usage, upload
    auto bar = b.removeFromTop (kToolbarHeight).reduced (8, 7);
    auto take = [&bar] (int w)
    {
        return bar.removeFromLeft (w).reduced (3, 0);
    };

    fileButton.setBounds (take (90));
    undoButton.setBounds (take (66));
    redoButton.setBounds (take (66));
    bar.removeFromLeft (14);

    uploadButton.setBounds (bar.removeFromRight (170).reduced (3, 0));

    durationLabel.setBounds (take (textWidth (durationLabel.getText()) + 12));
    durationEditor.setBounds (take (72));
    bar.removeFromLeft (8);
    repeatLabel.setBounds (take (textWidth (repeatLabel.getText()) + 12));
    repeatEditor.setBounds (take (56));
    bar.removeFromLeft (8);
    probeLabel.setBounds (take (textWidth (probeLabel.getText()) + 12));
    probeBox.setBounds (take (jmin (240, bar.getWidth())));
    bar.removeFromLeft (10);
    usageLabel.setBounds (bar);

    statusLabel.setBounds (b.removeFromBottom (kStatusHeight).reduced (10, 0));

    // left: probe map, full height
    probePanel.setBounds (b.removeFromLeft (kProbePanelWidth));
    b.removeFromLeft (1);

    // right: add bar, timeline, inspector
    addBar.setBounds (b.removeFromTop (addBar.preferredHeight (b.getWidth())));
    const int inspectorH = jmin (inspector.preferredHeight (b.getWidth()), b.getHeight() / 2);
    inspector.setBounds (b.removeFromBottom (inspectorH).withTrimmedTop (1));
    timeline.setBounds (b);
}

void PatternEditorComponent::showFileMenu()
{
    PopupMenu menu;
    menu.addItem (1, "New pattern");
    menu.addItem (2, "Open design (.json)...");
    menu.addSeparator();
    menu.addItem (3, "Save design");
    menu.addItem (4, "Save design as...");
    menu.addSeparator();
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
                                default: break;
                            }
                        });
}

bool PatternEditorComponent::keyPressed (const KeyPress& key)
{
    const auto cmd = ModifierKeys::commandModifier;
    if (key == KeyPress ('z', cmd, 0))
    {
        doc.undo();
        return true;
    }
    if (key == KeyPress ('z', cmd | ModifierKeys::shiftModifier, 0) || key == KeyPress ('y', cmd, 0))
    {
        doc.redo();
        return true;
    }
    if (key == KeyPress ('s', cmd, 0))
    {
        savePattern (false);
        return true;
    }
    if (key == KeyPress ('o', cmd, 0))
    {
        openPattern();
        return true;
    }
    return false;
}

void PatternEditorComponent::confirmDiscard (std::function<void()> then)
{
    if (! doc.dirty)
    {
        then();
        return;
    }

    AlertWindow::showOkCancelBox (AlertWindow::QuestionIcon, "Unsaved changes",
                                  "Discard the changes to " + documentTitle().upToLastOccurrenceOf (" *", false, false) + "?",
                                  "Discard", "Cancel", this,
                                  ModalCallbackFunction::create ([then] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         then();
                                                                 }));
}

void PatternEditorComponent::newPattern()
{
    confirmDiscard ([this]
                    {
                        Pattern p;
                        p.probe = doc.pattern().probe;
                        doc.reset (p, File());
                    });
}

void PatternEditorComponent::openPattern()
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
                                                  timeline.zoomToFit();
                                              });
                    });
}

void PatternEditorComponent::savePattern (bool askForFile)
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

    chooser = std::make_unique<FileChooser> ("Save pattern design", doc.file == File() ? File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("pattern.json") : doc.file, "*.json");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [write] (const FileChooser& fc)
                          {
                              if (fc.getResult() != File())
                                  write (fc.getResult());
                          });
}

void PatternEditorComponent::prepareOutput (const String& action, std::function<void (std::vector<std::uint8_t>)> onReady)
{
    const Pattern& p = doc.pattern();

    const auto problems = validate (p);
    if (! problems.empty())
    {
        String msg;
        for (const auto& s : problems)
            msg << "- " << String (s) << "\n";
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
                                  String ("The ") + probeMappingName (p.probe) + " channel mapping hasn't been verified on a device yet. Continue?",
                                  "Continue", "Cancel", this,
                                  ModalCallbackFunction::create ([generate] (int result)
                                                                 {
                                                                     if (result == 1)
                                                                         generate();
                                                                 }));
}

void PatternEditorComponent::exportBin()
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

void PatternEditorComponent::upload()
{
    if (! uploadCallback)
        return;

    prepareOutput ("Upload pattern", [this] (std::vector<std::uint8_t> bytes)
                   {
                       const File f = File::getSpecialLocation (File::tempDirectory).getChildFile ("HectoSTAR_pattern.bin");
                       f.deleteFile();
                       if (! f.appendData (bytes.data(), bytes.size()))
                       {
                           AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, "Upload pattern", "Couldn't write " + f.getFullPathName());
                           return;
                       }
                       uploadCallback (f, doc.pattern().repeatCount);
                   });
}

//==============================================================================

PatternEditorWindow::PatternEditorWindow (Component& host, ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload)
    : DocumentWindow ("HectoSTAR pattern editor", host.findColour (ThemeColours::componentBackground), DocumentWindow::allButtons)
{
    // same theme as the main Open Ephys window
    setLookAndFeel (&host.getLookAndFeel());

    editor = new PatternEditorComponent (initialProbe, std::move (onUpload));
    editor->onTitleChanged = [this] { setName ("HectoSTAR pattern editor - " + editor->documentTitle()); };
    setContentOwned (editor, false);
    setUsingNativeTitleBar (true);
    setResizable (true, false);

    // fill most of the screen Open Ephys is on (the timeline wants width, the lanes height)
    juce::Rectangle<int> screen (0, 0, 1600, 1000);
    if (const auto* display = Desktop::getInstance().getDisplays().getDisplayForRect (host.getScreenBounds()))
        screen = display->userArea;

    setResizeLimits (jmin (960, screen.getWidth()), jmin (600, screen.getHeight()), 10000, 10000);
    setBounds (screen.withSizeKeepingCentre ((int) (screen.getWidth() * 0.92), (int) (screen.getHeight() * 0.92)));
    setName ("HectoSTAR pattern editor - " + editor->documentTitle());
}

PatternEditorWindow::~PatternEditorWindow()
{
    clearContentComponent();
    setLookAndFeel (nullptr);
}

} // namespace PatternEditor
