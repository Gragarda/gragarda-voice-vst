# Gragarda Voice VST3 / standalone

**Gragarda Voice:** [OBS filter](https://github.com/Gragarda/gragarda-voice) · **VST3 / standalone** · [Web app](https://github.com/Gragarda/gragarda-voice-web) · [DSP core](https://github.com/Gragarda/gragarda-voice-core)

🇫🇷 [Version française](#version-française)

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/gragarda)

Gragarda Voice is free and open source. If it helps you, you can support its development on [Ko-fi](https://ko-fi.com/gragarda).

The [Gragarda Voice](https://github.com/Gragarda/gragarda-voice) voice chain as a VST3 plugin, for hosts other than OBS:
Wave Link, Voicemeeter, Reaper, FL Studio, Ableton Live, Bitwig, etc. Windows, macOS (universal) and Linux.

One plugin does it all: AI noise removal (DeepFilterNet 3 or RNNoise), high-pass, EQ, compressor, de-esser,
leveler (LUFS target per destination) and a -1 dB look-ahead limiter. Four macros: Clean, Warmth, Clarity, Punch
(50 = reference sound).

## Made with AI
To be transparent: **most of this plugin's code was written by an AI** (Claude, by Anthropic), under my direction.
I chose the processing chain, the settings and the features, and tested every version by ear.
The noise-removal engines it uses (DeepFilterNet, RNNoise) are third-party open-source projects not written by AI.
Bugs or doubts? Please open an [issue](../../issues).

## Install

Download the zip for your OS from the [releases](https://github.com/Gragarda/gragarda-voice-vst/releases)
(or the latest CI build artifacts) and copy `Gragarda Voice.vst3` into:

- Windows: `C:\Program Files\Common Files\VST3`
- macOS: `~/Library/Audio/Plug-Ins/VST3`
- Linux: `~/.vst3`

Then rescan plugins in your host.

### Standalone app (no DAW needed, Windows)

Run `gragarda-voice-setup-x.y.z.exe`: it installs the standalone app and the VST3. The "Install VB-Cable" option
(checked by default) downloads the free [VB-Cable](https://vb-audio.com/Cable/) from VB-Audio and runs its setup.
On first launch the app sends its output to "CABLE Input": pick "CABLE Output" as the microphone in Discord, OBS,
Teams, etc. The zip also holds the standalone app (`Gragarda Voice Standalone`, keep `Resources` next to the exe).

## Notes

- Fixed latency, reported to the host (compensated automatically in DAWs): Max quality 41.5 ms, Fast 21.5 ms.
  In live tools (Wave Link, Voicemeeter) add the same delay to your video if lip sync matters.
- The noise removers are trained at 48 kHz: use a 48 kHz project/device for best results.
- Calibration: the "Run calibration" button records 5 s of silence then 15 s of reading (the track/input must be
  receiving audio), shows 4 status lights and suggests gain, macros and a hidden mic correction (saved with the project).
- Language: the host does not pass its language; pick one in the list at the top right (Auto = system language).
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

---

# Version française

🇬🇧 [English version](#gragarda-voice-vst3--standalone)

**Gragarda Voice :** [Filtre OBS](https://github.com/Gragarda/gragarda-voice) · **VST3 / autonome** · [Application web](https://github.com/Gragarda/gragarda-voice-web) · [Cœur DSP](https://github.com/Gragarda/gragarda-voice-core)

[![Me soutenir sur Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/gragarda)

Gragarda Voice est gratuit et open source. S'il vous est utile, vous pouvez soutenir son développement sur [Ko-fi](https://ko-fi.com/gragarda).

La chaîne voix de [Gragarda Voice](https://github.com/Gragarda/gragarda-voice) en plugin VST3, pour les hôtes autres qu'OBS :
Wave Link, Voicemeeter, Reaper, FL Studio, Ableton Live, Bitwig, etc. Windows, macOS (universel) et Linux.

Un seul plugin fait tout : débruitage IA (DeepFilterNet 3 ou RNNoise), passe-haut, EQ, compresseur, de-esser,
leveler (cible LUFS selon la destination) et limiteur à anticipation −1 dB. Quatre macros : Nettoyage, Chaleur, Clarté, Punch
(50 = son de référence).

## Fait avec l'IA
Par transparence : **l'essentiel du code de ce plugin a été écrit par une IA** (Claude, d'Anthropic), sous ma direction.
J'ai choisi la chaîne de traitement, les réglages et les fonctionnalités, et j'ai testé chaque version à l'écoute.
Les moteurs de débruitage utilisés (DeepFilterNet, RNNoise) sont des projets open source tiers, non écrits par une IA.
Un bug ou un doute ? Ouvrez une [issue](../../issues).

## Installation

Télécharger le zip de votre système dans les [releases](https://github.com/Gragarda/gragarda-voice-vst/releases)
(ou les artefacts du dernier build CI) et copier `Gragarda Voice.vst3` dans :

- Windows : `C:\Program Files\Common Files\VST3`
- macOS : `~/Library/Audio/Plug-Ins/VST3`
- Linux : `~/.vst3`

Puis relancer le scan des plugins dans l'hôte.

### Application autonome (sans DAW, Windows)

Lancer `gragarda-voice-setup-x.y.z.exe` : il installe l'application autonome et le VST3. L'option « Installer VB-Cable »
(cochée par défaut) télécharge le [VB-Cable](https://vb-audio.com/Cable/) gratuit de VB-Audio et lance son installation.
Au premier lancement, l'application envoie sa sortie vers « CABLE Input » : choisir « CABLE Output » comme micro dans Discord, OBS,
Teams, etc. Le zip contient aussi l'application autonome (`Gragarda Voice Standalone`, garder `Resources` à côté de l'exe).

## Notes

- Latence fixe, signalée à l'hôte (compensée automatiquement dans les DAW) : Qualité max 41,5 ms, Rapide 21,5 ms.
  Dans les outils live (Wave Link, Voicemeeter), ajouter le même retard à la vidéo si la synchro labiale compte.
- Les débruiteurs sont entraînés à 48 kHz : utiliser un projet/périphérique à 48 kHz pour un meilleur résultat.
- Calibration : le bouton « Lancer la calibration » enregistre 5 s de silence puis 15 s de lecture (la piste/l'entrée doit
  recevoir du son), affiche 4 voyants et propose gain, macros et une correction cachée du micro (enregistrée avec le projet).
- Langue : l'hôte ne transmet pas sa langue ; en choisir une dans la liste en haut à droite (Auto = langue du système).
- Avant/après : joue le signal brut, retardé et à volume compensé, pour une comparaison honnête.

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

JUCE 8 est récupéré par CMake. Le cœur DSP est le submodule [gragarda-voice-core](https://github.com/Gragarda/gragarda-voice-core).

## Licence

Code sous GPL-2.0-or-later ; compilé avec JUCE (AGPLv3), le plugin distribué est donc sous AGPLv3.
RNNoise (BSD), DeepFilterNet (MIT/Apache-2.0) : voir `licenses/` dans le bundle.
