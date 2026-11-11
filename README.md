# Paragon PR7-T

[![Build](https://github.com/ARYEZ/Paragon-PR7-T/actions/workflows/build.yml/badge.svg)](https://github.com/ARYEZ/Paragon-PR7-T/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/ARYEZ/Paragon-PR7-T)](https://github.com/ARYEZ/Paragon-PR7-T/releases/latest)

The turntable instrument of the Paragon PR7 family: a VST3 instrument with (from 0.2.0) a deck to scratch, hold, spin
back, stop and drop the needle on, as the DJs of nu-metal bands played theirs, with PR7-K's synthesizer and effects.
Our own design and code, built with [JUCE](https://juce.com).

Paragon PR7-T starts from [Paragon PR7-K](https://github.com/ARYEZ/Paragon-PR7-K) 0.74.1, the family's synthesizer,
and shares the family's look, preset browser and undo. Its siblings are [PR7-G](https://github.com/ARYEZ/Paragon-PR7-G)
(guitar), [PR7-B](https://github.com/ARYEZ/Paragon-PR7-B) (bass), [PR7-V](https://github.com/ARYEZ/Paragon-PR7-V)
(vocal), [PR7-D](https://github.com/ARYEZ/Paragon-PR7-D) (drums), [PR7-C](https://github.com/ARYEZ/Paragon-PR7-C),
[PR7-E](https://github.com/ARYEZ/Paragon-PR7-E) (utility) and PR7-K (synth). It installs alongside all of them and keeps
its own presets.

## Status

PR7-T is built in stages, each one released:

| Stage | What | Now |
|---|---|---|
| **1** | Its own plugin: PR7-K 0.74.1 as "Paragon PR7-T", installing beside the others | **Works (0.1.0)** |
| 2 | The deck engine: a record played at whatever speed and direction the motor and your hand give it (backwards too), the motor's start and brake, BACKSPIN, HOLD, needle drops to four cues, the pitch wheel as your hand; clean at any speed | Next |
| 3 | The crossfader: its cut curve, reverse ("hamster"), clicks | Planned |
| 4 | The scratch library: baby, chirp, transformer, flare, crab, tear... as hand-and-fader moves, synced to tempo, played from keys | Planned |
| 5 | The platter view: a drawn turntable and fader to scratch live with the mouse | Planned |
| 6 | The sound: cartridge and mixer EQ, the mixer's filter and kill switches, lo-fi (with PR7-K's CRACKLE) | Planned |

## Using it

For now PR7-T is PR7-K 0.74.1 under its own name: its SYNTH, MOD, ARP, FX, PRESETS and SETTINGS pages, unchanged. See
[PR7-K's README](https://github.com/ARYEZ/Paragon-PR7-K#using-it). THE DECK comes in 0.2.0.

## Download

Get the latest version from the **[Releases](https://github.com/ARYEZ/Paragon-PR7-T/releases/latest)** page: download
the zip for your computer. (Builds of every push also appear under **Actions** → a run → **Artifacts**.)

Unzip it and copy `Paragon PR7-T.vst3` to your VST3 folder:

| OS | VST3 folder |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3` |
| Linux | `~/.vst3` |

Then rescan plugins in your DAW: PR7-T shows up as an instrument.

## Making a release

1. Set the new version in `CMakeLists.txt` (`project(ParagonX1 VERSION x.y.z)`).
2. Add a `## x.y.z` section to `CHANGELOG.md`.
3. Commit and push.

When a push carries a version that hasn't been released yet, GitHub Actions builds Windows and Linux, tags the
commit `vx.y.z` and publishes the release with notes from the changelog.

## Building from source

Requirements: CMake 3.24+, a C++20 compiler (Visual Studio 2022, Xcode 15+, or GCC 11+/Clang 14+), and Git.
JUCE and Catch2 are downloaded automatically at pinned versions.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The plugin is written to `build/ParagonX1_artefacts/Release/VST3/` (the build target keeps its family name).

On Linux, install JUCE's dependencies first (the list is in `.github/workflows/build.yml`).

To render the editor to an image without a DAW:

```bash
PARAGON_SNAPSHOT=editor.png ./build/tests/ParagonX1Tests "[snapshot]"
```

## License

Paragon PR7-T is free software under the [GNU Affero General Public License v3](LICENSE.md),
which is required by JUCE's open-source licence.

It uses:
- [JUCE](https://github.com/juce-framework/JUCE): AGPLv3
- The Inter typeface (in `resources/fonts`) by Rasmus Andersson, under the SIL Open Font License

THE HOUSE RECORD is synthesized by PR7-T itself: no one's recording is in it. PR7-T ships no samples from records;
the records you load are your own. The synthesizer is PR7-K's, inspired by VPS Avenger (Vengeance Sound's product);
PR7-T isn't affiliated with or endorsed by Vengeance Sound.
