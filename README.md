# Paragon PR7-T

[![Build](https://github.com/ARYEZ/Paragon-PR7-T/actions/workflows/build.yml/badge.svg)](https://github.com/ARYEZ/Paragon-PR7-T/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/ARYEZ/Paragon-PR7-T)](https://github.com/ARYEZ/Paragon-PR7-T/releases/latest)

The turntable instrument of the Paragon PR7 family: a VST3 instrument with a deck to scratch, hold, spin back, stop
and drop the needle on, as the DJs of nu-metal bands played theirs, with PR7-K's synthesizer and effects beside it.
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
| **2** | **The deck engine**: a record played at whatever speed and direction the motor and your hand give it (backwards too), the motor's start and brake, BACKSPIN, HOLD, needle drops to four cues, the pitch wheel as your hand; clean at any speed | **Works (0.2.0)** |
| **3** | **The crossfader**: its cut curve, reverse ("hamster"), CUT and TRANSFORM keys, a fader on the page | **Works (0.4.0)** |
| **4** | **The scratch library**: baby, chirp, transformer, flare, crab, tear... as hand-and-fader moves, synced to tempo, played from keys | **Works (0.5.0)** |
| 5 | The platter view: a drawn turntable and fader to scratch live with the mouse | **Started (0.3.0)**: our own 3D turntable in the family's "Tron" look, its platter turning and arm moving as it plays, the record grabbed with the mouse; the fader to come |
| **6** | **The sound**: the cartridge, the mixer channel's isolator EQ and kills, its filter, a worn record | **Works (0.6.0)** |
| 7 | **Into the FX**: the deck's sends to DELAY and REVERB, an insert bus; the record saved in presets | **Works (0.7.0)** |

## Using it

- **DECK** (the tab beside SYNTH): the turntable.
  - **The rack** (0.4.0, as PR7-I's): the deck's controls on one of the family's rack units across the top, in three
    pages (DECK, FADER, CUES, SCRATCH, MIXER, ROUTING); the screen shows the record and how it's turning; **ON**, **LOAD**, **HOUSE** under
    it; **DECK OUT**: the deck's meter and **LEVEL**. **HIDE RACK** gives the turntable the page's height.
  - **The sound** (0.6.0, the rack's MIXER page): **CART** (CLEAN, SCRATCH, WARM), the mixer channel's **LOW, MID,
    HIGH** isolator EQ with **KILL**s, the DJ **FILTER** (left low-pass, right high-pass) with **RES**, and **WEAR** (a
    worn record's hiss and crackle, moving with the record, and a warp). All neutral to start.
  - **The scratch library** (0.5.0): twelve techniques, one per key on **SCRATCH KEYS** (C2 by default): BABY,
    FORWARD, BACKWARD, CHIRP, STAB, DRAG, TEAR, SCRIBBLE, TRANSFORMER, FLARE, 2-CLICK FLARE, CRAB. Each in time with
    your song (**RATE**: half, normal, double), its stroke as long as the key is hard (up to **REACH**), repeating
    while held. 0.8.0 tuned them to measurements of real DJs (K. F. Hansen's research at KTH). **MOD HAND** makes the mod wheel a hand on the record. The rack's SCRATCH page lists them: click one
    to play it.
  - **The crossfader** (0.4.0): **FADER** (open by default), **CURVE** (SMOOTH to a scratch CUT), **HAMSTER** (turned
    round), the **CUT** key (F#: flips it while held) and **TRANSFORM** (G#: chops it in time with your song at
    TRANSFORM's rate). Drag the fader under the record, or MIDI-learn FADER (right-click) to a controller's fader.
  - **ON**: the deck's on or off. **Off by default**, so every preset plays as it does on PR7-K. On, the deck's 12
    keys (**KEYS**, from C1 by default, the on-screen keyboard's lowest octave) and the **pitch wheel** are the deck's
    and not the synth's.
  - **The keys**: **PLAY** (C) starts the motor, **STOP** (C#) stops it, **BACKSPIN** (D) throws the record back,
    **HOLD** (D#, while held) is a hand holding the record still, **DROP 1-4** (E, F, G, A) drop the needle at
    **CUE 1-4**, the motor on. The same keys are on the page as buttons.
  - **Your hand**: the **pitch wheel** moves the record, forwards up and backwards down, as far as **REACH** (in turns
    of the platter; a turn is 1.8 seconds of the record). Let the wheel spring back and the record comes back with
    it: a baby scratch. Left at its centre a moment, the hand lets go and the motor takes the record again. Or grab
    the **platter** on the page with the mouse and drag round it.
  - **The record**: **LOAD RECORD...** puts a sample on (WAV, AIFF, FLAC or Ogg, up to 3 minutes; the project
    remembers it). **HOUSE RECORD** is ours, built in: an "ahh", a horn stab, a drum break and a 1 kHz tone, two
    seconds apart, where the four cues start. The strip shows the record, the needle's place and the cues.
  - **The knobs**: **PITCH** (the pitch fader, +-16%), **RPM** (33 or 45), **START** and **BRAKE** (the motor up to
    speed and down to a stop; a long BRAKE is a tape stop), **SPIN** and **SPIN TIME** (BACKSPIN's throw and how long
    it takes to stop), **REACH**, **NEEDLE** (the drop's thump and crackle), **CUE 1-4**, **LEVEL**.
  - **The turntable** (0.3.0): our own deck in the family's "Tron" look (as PR7-D's kit, PR7-I's guitar and PR7-O's
    strings): the platter turns as the record does, the arm moves in as it plays through, the neon brightens as it
    turns. Drag round the record with the mouse to scratch.
  - The deck goes through everything the synth does after it: the rack (FX), OUTPUT and the LIMITER.
  - **Routing** (0.7.0, the rack's ROUTING page): **TO DELAY** and **TO REVERB** send the deck (after its crossfader,
    so the echoes ring on after a cut) to the FX page's DELAY and REVERB (turn them on in FX > SENDS); **INSERT**
    plays the deck into insert bus A-D instead of the main mix, through that bus's effects.
  - **Presets** (0.7.0) remember the deck's record (the file's place, or the house record) and put it back.
  - Like a real turntable, the record's loudness follows its speed: a slow drag is quiet, a fast scratch loud (up to
    +6 dB), a record held still silent.
- **SYNTH, MOD, ARP, FX, PRESETS, SETTINGS**: PR7-K's, unchanged. See
  [PR7-K's README](https://github.com/ARYEZ/Paragon-PR7-K#using-it).

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

To render the editor to an image without a DAW (`PARAGON_SNAPSHOT_PAGE=deck` for THE DECK):

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
