# Gragarda Voice VST3

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/gragarda)

The [Gragarda Voice](https://github.com/Gragarda/gragarda-voice) voice chain as a VST3 plugin, for hosts other than OBS:
Wave Link, Voicemeeter, Reaper, FL Studio, Ableton Live, Bitwig, etc. Windows, macOS (universal) and Linux.

One plugin does it all: AI noise removal (DeepFilterNet 3 or RNNoise), high-pass, EQ, compressor, de-esser,
leveler (LUFS target per destination) and a -1 dB look-ahead limiter. Four macros: Clean, Warmth, Clarity, Punch
(50 = reference sound).

## Install

Download the zip for your OS from the [releases](https://github.com/Gragarda/gragarda-voice-vst/releases)
(or the latest CI build artifacts) and copy `Gragarda Voice.vst3` into:

- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `~/Library/Audio/Plug-Ins/VST3`
- Linux: `~/.vst3`

Then rescan plugins in your host.

## Notes

- Fixed latency, reported to the host (compensated automatically in DAWs): Max quality 41.5 ms, Fast 21.5 ms.
  In live tools (Wave Link, Voicemeeter) add the same delay to your video if lip sync matters.
- The noise removers are trained at 48 kHz: use a 48 kHz project/device for best results.
- Before/after: plays the raw signal, delayed and level matched, for an honest comparison.

## Build

```sh
git clone --recursive https://github.com/Gragarda/gragarda-voice-vst
git clone https://github.com/Rikorose/DeepFilterNet.git DeepFilterNet
cd DeepFilterNet && git checkout d375b2d
cargo build --release -p deep_filter --no-default-features --features capi --lib
cd ../gragarda-voice-vst
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8 is fetched by CMake. The DSP core is the [gragarda-voice-core](https://github.com/Gragarda/gragarda-voice-core)
submodule.

## License

GPL-2.0-or-later code; built with JUCE (AGPLv3), the distributed plugin is therefore under AGPLv3.
RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0): see `licenses/` in the bundle.
