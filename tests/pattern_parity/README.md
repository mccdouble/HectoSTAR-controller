# Pattern generator parity test

Checks that the plugin's C++ pattern generator (`Source/PatternGen`) writes
exactly the same `.bin` files as the MATLAB tool it replaces
(`tools/matlab/PatternGenGUI.m`), byte for byte.

The MATLAB side runs the generator functions taken straight out of
`PatternGenGUI.m` (not a copy), plus the pad / remap / write steps of its
"Generate File" button. Needs MATLAB with the Signal Processing Toolbox
(`square`, `sawtooth`) and Python 3.

## Run

From this folder, with `OUT` set to any scratch folder:

```bash
python make_cases.py $OUT/cases 150
cmake -S . -B $OUT/build -G "Visual Studio 17 2022" -A x64
cmake --build $OUT/build --config Release
$OUT/build/Release/parity_runner $OUT/cases $OUT/out_cpp
matlab -batch "run_reference('../../tools/matlab/PatternGenGUI.m','$OUT/cases','$OUT/out_matlab')"
python compare.py $OUT/out_matlab $OUT/out_cpp
```

`compare.py` exits non-zero and lists the first differing byte of each
mismatched case.

Then the pattern model tests (`PatternModel`, `Formula`, `Json`), which also
re-check MATLAB equivalence through the block model and a JSON round trip:

```bash
$OUT/build/Release/model_tests $OUT/cases $OUT/out_matlab
```

## Cases

`make_cases.py` writes hand-made cases (`h_*`: one per waveform type, duty
0/100, amplitudes over 100 uA, negative steps, even wavelet peaks, zero
periods, NaN/Inf parameters, delays past the end, padding of 1-7 samples, a
distinct value per channel for each device mapping, and a 64-LED stagger)
and seeded random cases (`r_*`).

## Notes on exactness

Matching MATLAB to the byte depends on reproducing its floating-point
`mod`/`rem` (fmod, but a quotient within `eps` of an integer gives exactly
0) and the Signal Processing Toolbox `square`/`sawtooth`, and on evaluating
every expression in the same order. See the comments in
`PatternGenerator.cpp`.

Last run (2026-11-04, MATLAB R2025b, MSVC 17 x64): 179 of 179 cases
identical, 36,555,520 bytes compared; model tests 113 of 113 checks passed
(179 of 179 MATLAB-identical through the block model and after JSON reload).
