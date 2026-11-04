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
2. **Pattern model (done).** `PatternModel` (blocks on LED lanes, higher
   value wins on overlap, fades), custom waveforms (`Breakpoints`,
   `Samples`, `Formula`), and `.json` save/load (`Json`). A design with one
   built-in block per LED renders exactly what MATLAB does (checked by
   `model_tests`, also after a JSON round trip).
3. **Editor window (first version).** `Source/UI/PatternEditor/`, opened
   with "Pattern Editor..." in the stim panel. Probe map + sequencing tools
   (add blocks to the selected LEDs, stagger) on the left, timeline in the
   middle, inspector below, top bar for duration / probe / repeats / flash
   usage, `.json` open/save, `.bin` export and upload. Snapshot undo/redo.
   Upload goes through the background pattern upload, then selects Pattern
   Output with the pattern's repeat count.
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

## Pattern model (phase 2)

- A block plays only inside `[start, start + length)`; samples are
  included when their time is in that window. Built-in types use the block
  start as MATLAB's delay, so their math is unchanged.
- Rendering per block: compute the current in uA, apply fades (linear gain
  from the start / to the end), clip to 0..100 and round to a DAC step, then
  take the max with what is already on that LED.
- Custom waveforms:
  - **Breakpoints**: `[time, value]` points from the block start, joined by
    step, linear or smooth (cosine) segments. Without repeat, 0 outside the
    first..last point; with repeat, tiles every last-point-time seconds.
  - **Samples**: values at 20 kHz from the block start (e.g. a CSV import),
    0 after the end unless repeated.
  - **Formula**: a MATLAB-flavoured expression of `t` (s since block
    start), `T` (block length), `fs`, with MATLAB precedence (`-2^2 = -4`,
    left-associative `^`), element-wise spellings (`.*`), comparisons and
    logic (1/0), and functions including `mod`, `rem`, `round`, `square`,
    `sawtooth` that match MATLAB exactly. See `Formula.h`.
- `naturalLength` gives the length a waveform implies (count x period,
  wavelet D, last breakpoint, sample count) so the editor can size new
  blocks; 0 means "until the block ends".
- `validate` reports anything that stops generation: duration, LEDs out of
  1-64, formula errors, unordered breakpoints, flash capacity.

### Design file (`.json`)

```json
{
  "format": "hectostar-pattern",
  "version": 1,
  "duration": 0.5,
  "probe": "4 Shank Acute",
  "repeatCount": 1,
  "blocks": [
    {
      "id": 1, "led": 5, "start": 0.01, "length": 0.1, "link": 2,
      "label": "10 Hz train", "fadeIn": 0.002,
      "wave": {"kind": "builtin", "type": "fixedpulse",
               "freq": 10, "amp": 60, "offset": 0.01, "duty": 50, "step": 0}
    },
    {"id": 2, "led": 6, "start": 0, "length": 0.2,
     "wave": {"kind": "breakpoints", "interpolation": "linear", "repeat": true,
              "points": [[0, 0], [0.005, 80], [0.01, 0]]}},
    {"id": 3, "led": 7, "start": 0, "length": 0.2,
     "wave": {"kind": "formula", "expression": "50*(1+sin(2*pi*8*t))"}},
    {"id": 4, "led": 8, "start": 0.05, "length": 0.01,
     "wave": {"kind": "samples", "repeat": false, "values": [10, 20, 30]}}
  ]
}
```

Built-in fields keep the MATLAB table's meanings (`freq` = "Freq/#Pulses",
`offset` = "Offset/Period", `duty` = "Duty (%) / Cycles"). Numbers are
written with full precision; NaN/Inf parameters are written as `null`.

## Pulse shapes (shaped trains)

The main waveform list is a dictionary of pulse shapes. Each block of this
kind is a train: `count` pulses, one starting every `1/rate` s, each `width`
s long, at `amp` uA peak (plus `step` uA per following pulse). With
`equalCharge`, the peak is scaled so each pulse carries the charge of a
square pulse at `amp` (the iso-energy condition of the paper below); a
pattern needing more than 100 uA is reported, not clipped.

The first twelve shapes are Table 6 of Bansal, Pyari and Roy 2023,
"Optogenetic generation of neural firing patterns with temporal shaping of
light pulses", Photonics 10(5):571 (doi:10.3390/photonics10050571):

| Editor name           | Paper name                 | Shape over x = 0..1            |
|-----------------------|----------------------------|--------------------------------|
| Square pulses         | square                     | 1                              |
| Ramp up               | left-triangular            | x                              |
| Ramp down             | right-triangular           | 1 - x                          |
| Triangle              | triangular                 | 2x, then 2(1 - x)              |
| Ramp up, then hold    | backward-ramp              | 2x, then 1                     |
| Hold, then ramp down  | forward-ramp               | 1, then 2(1 - x)               |
| Gaussian              | Gaussian                   | exp(-(x-0.5)^2 / 2s^2), s = 1/7 |
| Rising half-Gaussian  | left-Gaussian              | centre at x = 1, s = 1/3.5     |
| Falling half-Gaussian | right-Gaussian             | centre at x = 0, s = 1/3.5     |
| Half-sine             | positive-sinusoidal        | sin(pi x)                      |
| Rising quarter-sine   | left-positive-sinusoidal   | sin(pi x / 2)                  |
| Falling quarter-sine  | right-positive-sinusoidal  | cos(pi x / 2)                  |
| Sine cycles           | (added)                    | (1 - cos 2 pi x) / 2           |

Samples are taken at the middle of each 50 us step, except that rising
shapes end exactly at the peak and falling shapes start at it. The paper's
"left/right" names are kept in the editor's help text. Note the paper lists
iso-energy irradiance 2.635x for the Gaussians; its sigma = width/7 gives
2.79x, which is what equal charge uses (computed from the rendered samples).
