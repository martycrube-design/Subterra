# Subterra

A bass synthesizer pedal as a VST3 / AU plugin. Plug a bass in and play: Subterra tracks the pitch and
dynamics of each note and plays a synth voice along with it, blended with your dry bass.

![Subterra UI](docs/subterra-ui.png)

## Controls

Every control on the UI is a real, automatable parameter.

| Control | What it does |
| --- | --- |
| **Wave** | Oscillator shape, a continuous morph from sine to saw to square to triangle (the icons around the knob). |
| **Octave** | Synth pitch relative to the note you play: -2, -1, 0, +1, +2 octaves. |
| **Filter** | Cutoff of the synth's resonant low-pass filter (40 Hz to 12 kHz). |
| **Reso** | Filter resonance. |
| **Env** | How far your picking dynamics sweep the filter. Right of centre opens it as you dig in, left closes it. |
| **Drive** | Saturation on the synth voice. |
| **Mix** | Balance between your dry bass and the synth (equal-power). |
| **Bypass** footswitch | True bypass with a click-free crossfade. The left LED is lit when the pedal is on. Also the host's bypass. |
| **Hold** footswitch | Freezes the current synth note so it drones on after you stop playing. The right LED shows Hold. |
| **Input / Output** | Gain into the pedal and final level, ±24 dB, with stereo peak meters. |
| **In Filters** | High- and low-pass on the bass before the pedal (also cleans up what the tracker hears). OFF/ON switch. |
| **Out Filters** | High- and low-pass on the final output. OFF/ON switch. |
| **Tracking** | Sensitivity: how quietly you can play and still trigger the synth. |
| **Glide** | Portamento between legato notes (off to 1 s). Notes that start from silence always land on pitch. |
| **Playing** readout | The note and frequency you are playing. |
| **Tuner** | Turns the readout into a tuner (note, cents and a needle) and mutes the output. Muting and the A4 reference are in Settings. |
| **Presets** | ‹ › step through presets; click the name or the folder button to pick from the list. The save button stores your own. A `*` means the sound was edited. |
| **A/B** | Click to switch between two complete settings to compare them. Right-click to copy one to the other. |
| **Settings** | Tuner reference (A4 = 432 to 446 Hz), mute while tuning, window size, reset. |

Knobs: drag up/down, mouse wheel, double-click to reset. Hover for a description, and the value shows while you drag.
The window can be resized from the corner.

## How it works

- **Pitch**: the input is low-passed, decimated to about 8 kHz and analysed with the YIN algorithm every ~6 ms,
  with a median filter against octave slips. It tracks B0 (31 Hz) to 1 kHz to within a fraction of a cent and
  locks onto a low E in about 65 ms.
- **Dynamics**: a two-pole mean-square follower gives a smooth level that drives the synth's volume and the
  filter envelope, so the synth breathes with your playing.
- **Voice**: band-limited (PolyBLEP) morphing oscillator, tanh drive, and a zero-delay-feedback state-variable
  low-pass filter.

Source layout: `source/dsp` (tracker, voice), `source/PluginProcessor.*` (signal flow), `source/Presets.*`,
`source/PluginEditor.*` and `source/ui` (the interface), `tests/` (engine tests), `tools/Snapshot.cpp`
(UI screenshot and control check).

## Building

Requires CMake 3.22+ and a C++20 compiler. JUCE 8 is fetched automatically (or pass `-DJUCE_DIR=/path/to/JUCE`).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/SubterraTests_artefacts/Release/SubterraTests
```

macOS builds VST3, AU and a standalone app; Windows and Linux build VST3 and standalone.

GitHub Actions builds all three platforms on every push, runs the engine tests, checks every UI control,
and validates the plugins with [pluginval](https://github.com/Tracktion/pluginval) at its strictest level
(plus `auval` for the Audio Unit). Download the builds from the run's **Artifacts**: on a Mac, run `Subterra-macOS-Installer.pkg`. Pushing a tag such as
`v0.1.0` also publishes them as a GitHub release.

## Licence notes

Built with [JUCE](https://juce.com) 8. Using JUCE requires either a JUCE licence or releasing under the AGPLv3,
so choose a licence before distributing builds. Fonts: Bebas Neue and Inter, both under the SIL Open Font
Licence (see `resources/fonts`).
