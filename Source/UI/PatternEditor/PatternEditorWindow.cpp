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
      timeline (doc),
      inspector (doc)
{
    setWantsKeyboardFocus (true);

    Pattern p;
    p.probe = initialProbe;
    doc.reset (p, File());

    for (auto* b : { &newButton, &openButton, &saveButton, &saveAsButton, &undoButton, &redoButton, &fitButton, &exportButton, &uploadButton })
        addAndMakeVisible (b);

    newButton.onClick = [this] { newPattern(); };
    openButton.onClick = [this] { openPattern(); };
    saveButton.onClick = [this] { savePattern (false); };
    saveAsButton.onClick = [this] { savePattern (true); };
    undoButton.onClick = [this] { doc.undo(); };
    redoButton.onClick = [this] { doc.redo(); };
    fitButton.onClick = [this] { timeline.zoomToFit(); };
    exportButton.onClick = [this] { exportBin(); };
    uploadButton.onClick = [this] { upload(); };
    uploadButton.setTooltip ("Generate the pattern file and write it to the board's flash (Pattern Output is selected afterwards)");

    for (auto* l : { &durationLabel, &probeLabel, &repeatLabel })
    {
        l->setFont (FontOptions (12.0f));
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }

    durationEditor.setInputRestrictions (10, "0123456789.");
    durationEditor.setJustification (Justification::centredRight);
    durationEditor.onReturnKey = durationEditor.onFocusLost = [this]
    {
        const double v = durationEditor.getText().getDoubleValue();
        if (v > 0 && v != doc.pattern().duration)
            doc.modify ([v] (Pattern& p) { p.duration = v; });
        else
            refresh();
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

    usageLabel.setFont (FontOptions (12.0f));
    addAndMakeVisible (usageLabel);

    addAndMakeVisible (probePanel);
    addAndMakeVisible (timeline);
    addAndMakeVisible (inspector);

    statusLabel.setFont (FontOptions (12.0f));
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

    undoButton.setEnabled (doc.canUndo());
    redoButton.setEnabled (doc.canRedo());

    const double maxSeconds = (double) kMaxSamples / kSampleRateHz;
    const double mb = encodedSizeBytes (p) / (1024.0 * 1024.0);
    String usage;
    usage << String (mb, 2) << " MB of 16 MB flash (" << formatSeconds (p.duration) << " of " << formatSeconds (maxSeconds) << ")";
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
        statusLabel.setColour (Label::textColourId, findColour (ThemeColours::defaultText).withAlpha (0.7f));
        statusLabel.setText (String ((int) p.blocks.size()) + " blocks on " + String ((int) activeLeds (p).size())
                                 + " LEDs.  Double-click a lane to add a block; drag to move, drag an edge to resize (Alt: single samples); "
                                   "Ctrl+wheel to zoom; Delete removes; Ctrl+Z / Ctrl+Y undo / redo.",
                             dontSendNotification);
    }

    if (onTitleChanged)
        onTitleChanged();
}

void PatternEditorComponent::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
}

void PatternEditorComponent::resized()
{
    auto b = getLocalBounds();

    auto top = b.removeFromTop (36).reduced (6, 6);
    for (auto* btn : { &newButton, &openButton, &saveButton, &saveAsButton })
        btn->setBounds (top.removeFromLeft (74).reduced (2, 0));
    top.removeFromLeft (8);
    undoButton.setBounds (top.removeFromLeft (56).reduced (2, 0));
    redoButton.setBounds (top.removeFromLeft (56).reduced (2, 0));
    fitButton.setBounds (top.removeFromLeft (90).reduced (2, 0));
    top.removeFromLeft (8);
    durationLabel.setBounds (top.removeFromLeft (80));
    durationEditor.setBounds (top.removeFromLeft (64).reduced (2, 0));
    probeLabel.setBounds (top.removeFromLeft (44));
    probeBox.setBounds (top.removeFromLeft (200).reduced (2, 0));
    repeatLabel.setBounds (top.removeFromLeft (58));
    repeatEditor.setBounds (top.removeFromLeft (50).reduced (2, 0));
    uploadButton.setBounds (top.removeFromRight (124).reduced (2, 0));
    exportButton.setBounds (top.removeFromRight (104).reduced (2, 0));
    usageLabel.setBounds (top.withTrimmedLeft (8));

    statusLabel.setBounds (b.removeFromBottom (22).reduced (8, 0));
    inspector.setBounds (b.removeFromBottom (124));
    probePanel.setBounds (b.removeFromLeft (230));
    timeline.setBounds (b);
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

PatternEditorWindow::PatternEditorWindow (ProbeMapping initialProbe, std::function<void (const File&, int)> onUpload)
    : DocumentWindow ("HectoSTAR pattern editor", Colours::darkgrey, DocumentWindow::allButtons)
{
    editor = new PatternEditorComponent (initialProbe, std::move (onUpload));
    editor->onTitleChanged = [this] { setName ("HectoSTAR pattern editor - " + editor->documentTitle()); };
    setContentOwned (editor, false);
    setUsingNativeTitleBar (true);
    setResizable (true, false);
    setResizeLimits (900, 560, 4000, 3000);
    centreWithSize (1280, 780);
    setName ("HectoSTAR pattern editor - " + editor->documentTitle());
}

} // namespace PatternEditor
