# BBD SCF04 Model (VST3)

Circuit-based model of the SCF 04-L schematic (TCchorusA.pdf / TCchorusB.pdf).
Unofficial: not affiliated with or endorsed by TC Electronic.

## Build
    cmake -B build -DJUCE_DIR=/path/to/JUCE      # or omit to fetch JUCE 8.0.4
    cmake --build build --config Release
The VST3 ends up in build/BBD_SCF04_Model_artefacts/.

## Test the DSP without JUCE
    cd Test && g++ -std=c++17 -O2 -I../Source engine_test.cpp -o engine_test && ./engine_test

## Status of each part
| Block | Source | Confidence |
|---|---|---|
| Topology: input buffer/clip, NE570 compander, clock-tracking LPFs, 1024-stage BBD, LFO-driven clock, delay = 512/f_clk | schematic | High |
| Mode behaviour: Chorus = dry + intensity*wet, Flanger = dry + wet with intensity as feedback, PM = 100% wet | schematic switch table + SOS review | Medium |
| Pre/post filter corner and Q (20.6 kHz / 13.75 kHz, Q 0.73) | R/C values read from scan | Medium (scan is low-res) |
| Clock-tracking law of the LM13600 filters | not legible | Placeholder (`filterTrack`) |
| Centre delay, clock swing, LFO shape, flanger feedback max, per mode | needs VCO network trace or measurement | Placeholder (`Calibration::mode`) |
| Compander reference level and detector time constant | partly legible (C7, C8) | Placeholder |
| Output gain staging, R-channel polarity | inferred | Placeholder |

## How to calibrate against your pedal
1. Scope the clock at IC4 (circled nodes 5/6 on page A): note f_clk at LFO min/max for each mode with WIDTH at 0 and 100%.
   centerDelayMs = 512 / f_centre; clockDepth = (f_max - f_min) / (f_max + f_min).
2. Measure LFO rate range at SPEED min/max and edit the `speed` range in PluginProcessor.cpp.
3. Measure flanger feedback by comparing successive comb peaks at INTENSITY max -> `feedbackMax`.
4. Edit values in `scf::Calibration` (ScfEngine.h); nothing else needs to change.

## No compiler? Build in the cloud
1. Create a free GitHub account and a new repository.
2. Upload everything in this folder (including the hidden .github folder).
3. Open the Actions tab -> "Build VST3" -> Run workflow.
4. When it finishes, download the artifact for your OS and copy the
   "BBD SCF04 Model.vst3" into your VST3 folder
   (Windows: C:\Program Files\Common Files\VST3, macOS: /Library/Audio/Plug-Ins/VST3).
