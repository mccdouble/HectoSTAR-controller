# Pattern editor design

Working notes for bringing pattern stim generation (previously the MATLAB
tool `tools/matlab/PatternGenGUI.m`) into the HectoSTAR plugin. Pulse stim
output stays as it is; the pattern editor is an additional tool.

## Pattern file format

What the FPGA plays back from flash:

- 20 kHz sample rate. Every sample is a 64-byte frame, one byte per LED, so
  all 64 LEDs are commanded every 50 us whether they are used or not. File
  size depends only on duration (1.28 MB per second).
- Each byte is a DAC current in uA: 0 to 100 uA in 1 uA steps.
- Frame byte *i* carries GUI channel `reorder(i)` for the target probe
  (per-device remapping, `getReorderForDevice`).
- The sample count is zero-padded to a multiple of 4, so the file is a whole
  number of 256-byte flash pages (the FPGA's page count is
  `patternLength / 256`).
- The pattern flash is 16 MB, so about 13 s of pattern. Repeating a short
  pattern with the plugin's repeat count extends total stimulation time
  without using more flash. (A move to the board's RAM chip is possible
  later; not planned.)

Probe mappings verified on hardware: 4-Shank Chronic, 4-Shank Acute, Demo
Board. 2-Shank Chronic is not yet verified and must be labelled as such.

## Build order

1. **Generator and file writer (done).** `Source/PatternGen/` ports the
   MATLAB waveform backend and "Generate File" step to plain C++17 (no JUCE),
   verified byte for byte against MATLAB by `tests/pattern_parity`.
2. **Pattern model.** Blocks on LED lanes rendered through the generator;
   save and load the design as `.json` (the `.bin` is not editable).
3. **Editor window.** Timeline, probe map, inspector, sequencing tools.
4. **Preview and feedback.** Waveform drawing in blocks, playback animation
   on the probe map, flash capacity and size readouts.
5. **Output.** Save `.bin`, and generate and upload to the board in one step
   through the background pattern upload.

## Agreed design

- **Layout**: DAW-style. A probe map to select LEDs, a timeline with one
  lane per LED, and an inspector for the selected blocks. Only LEDs that are
  active in the pattern get a lane (not all 64).
- **Blocks**: a waveform on one LED starting at a given time. Count-based
  types (fixed pulse, fixed sine, fixed sawtooth, staircase, wavelet) take
  their length from count x period; continuous types (pulse, sine,
  sawtooth, double sine) are stretched to length. A block placed on several
  LEDs creates linked copies that can be edited together or unlinked.
- **Overlap**: where blocks overlap on one LED, the higher value wins,
  clipped to 0-100 uA.
- **Typical use**: many LEDs with the same waveform offset in time, so
  place-on-selection and stagger/sweep are first-class tools. Different
  waveforms on different LEDs at the same time must also work.
- **Units**: currents in uA everywhere (never "intensity %"), times in
  seconds/ms. Times snap to the 50 us sample period.
- **Presets and raw parameters**: presets are named, saved blocks
  ("10 Hz, 5 ms, 60 uA") that can still be edited at the parameter level.
- **Custom waveforms** (to explore): a breakpoint editor (step, linear or
  smooth, optionally repeating one period), CSV sample import, envelope /
  ramp modifiers that scale any block, and formula entry
  (e.g. `50*(1+sin(2*pi*8*t))`). Custom shapes live in a waveform library
  alongside presets.
- **Capacity**: the editor shows how much of the ~13 s of flash is used and
  won't generate a pattern that does not fit; repeat count is shown with the
  total stimulation time it gives.
- The generator and file writer stay separate from the UI so a future
  format change (RAM chip, compression, rate) only touches the writer.
