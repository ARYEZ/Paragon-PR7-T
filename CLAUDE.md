# Paragon PR7-T: notes for Claude

This repository starts as a copy of **Paragon PR7-K** (the synthesizer, repo `ARYEZ/Paragon-PR7-K`) at PR7-K **0.74.1** (`main`, commit `62dd8a2`), full history included, to become **Paragon PR7-T**, the **turntable instrument** of the PR7 family: a VST3 instrument whose deck is scratched, held, spun back, stopped and needle-dropped as the DJs of nu-metal bands played theirs (the owner's ask: "focus on DJ turntables and the way they were used in Nu-metal songs"). PR7-K's synthesizer, effects and presets stay in it, unchanged (the owner chose to fork from PR7-K; whether PR7-T later loses the synth, as PR7-D did, is the owner's call). Siblings: **PR7-G** (guitar, `ARYEZ/Paragon-PR7-G`), **PR7-B** (bass), **PR7-V** (vocal), **PR7-D** (drums), **PR7-C**, **PR7-E** (utility), **PR7-K** (synth). PR7-K's own history (what each of its versions did, and why) is in PR7-K's CLAUDE.md; everything below describes PR7-T, and how the project is built, tested and released, and how the owner likes to work.

## First: make it its own plugin (done in 0.1.0)

All of this was done in 0.1.0; it's kept as the record of what makes PR7-T its own plugin.

1. **CMakeLists.txt** (`juce_add_plugin`): `PRODUCT_NAME` "Paragon PR7-T", `BUNDLE_ID` `com.paragon.paragonpr7t`, `PLUGIN_CODE` `Pr7t` (never another sibling's: `Px1a`, `Pr7b`, `Pr7v`, `Pr7c`, `Pr7e`, `Pr7k`, `Pr7d`), `PLUGIN_MANUFACTURER_CODE Prgn`; an instrument (`IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `VST3_CATEGORIES Instrument Sampler`: it plays records); version restarted at 0.1.0 (the CMake target name stays `ParagonX1`).
2. **Presets folder** (`source/rigs/RigLibrary.cpp`): "Paragon PR7-T" (tested in `tests/RigTests.cpp`). Nothing carries over from PR7-K's folder.
3. **Settings file** (`source/ui/Finish.cpp`): `applicationName` "Paragon PR7-T".
4. **Logo** (`source/ui/Logo.cpp`): "-T", built from the P's parts (the 7's 0.2 top bar, a 0.24 stem upright down the middle; one outline: the path fills even-odd), in the same slanted, speed-cut lettering.
5. **Text the user sees:** About (SETTINGS), tooltips, the rack units' "PR7-T SERIES" plates, the library download window, the presets' notes, README; thread names. Comments that record where code came from ("(PR7-K: one exp for both)") stay as they are: they're its history.
6. **Release workflow:** `Paragon PR7-T.vst3`, `ParagonPR7-T-${TAG}-...` zips, "Paragon PR7-T ${TAG}". Releases were off in the seed commit (it still built PR7-K 0.74.1) and back on in 0.1.0.
7. **CHANGELOG.md** starts at 0.1.0.

## The plan: a turntable, built in stages

What the DJs of nu-metal bands did with theirs, and so what PR7-T is built to do: scratches (baby, chirp, transformer, flare, crab, tear: each the record hand and the crossfader together), stabs and drops (a short hit of a sample on the riff), spinbacks, brakes and tape stops at the ends of sections, needle drops, and noise (the mixer's filter, feedback-like textures). PR7-T ships no one's recordings: the records are the user's own samples, or THE HOUSE RECORD (synthesized by PR7-T).

1. **Its own plugin** (0.1.0, done).
2. **THE DECK, the engine** (0.2.0): a record played at whatever speed and direction the motor and a hand give it (backwards too), the motor's start and brake, backspin, hold, needle drops; clean at any speed (no aliasing).
3. **The crossfader**: its cut curve, reverse ("hamster"), click patterns. Half of every scratch is the fader.
4. **The scratch library**: each technique as a hand-motion curve plus fader cuts, synced to tempo; a key plays one, velocity its intensity; the mod wheel can take the record by hand.
5. **The platter view**: a drawn turntable and fader to scratch live with the mouse, glowing as it plays.
6. **The sound**: cartridge and mixer EQ, the mixer's filter and kill switches, lo-fi (PR7-K's CRACKLE is already in the rack).

## The project

- An AGPLv3 **JUCE VST3** instrument, a turntable (to come) with PR7-K's synthesizer, in the PR7 family's look, sharing the family's code.
- **Code layout:** the synth in `source/dsp/synth/`, PR7-E's effects (for stage 7) in `source/dsp/effects/`, `dsp/eq/`, `dsp/dominion/`; UI in `source/ui/`, parameters in `source/Parameters.*`, presets ("rigs" in the code, as across the family) in `source/rigs/`, tests in `tests/`.
- **Parameters:**
  - Saved by ID and choices by **index**: only ever *append* choices, never reorder.
  - New parameters get a new version hint number.
  - Migrations for changed defaults live in `setStateInformation` and `Rig::fromXml`.
- **Shared DSP pieces:** `BypassFade` and `TailWatch` (EffectUtils.h), and `Circuit` (Circuit.h: models analog stages from their schematics, part by part).

## Speed (PR7-K 0.68.0, the owner's ask: lighter, the sound unchanged)

Each change keeps every sum and its order (only independent work overlapped, or work multiplied by exactly 0 skipped), proved by `tests/SpeedTests.cpp` (`[.speed]`, `PARAGON_SPEED_DIR=<folder>`: eight held-chord renders of the heavy effects written raw, compared byte for byte against a build of the code before; all eight identical) and timed back to back: plain chord 1.28% -> 0.96% (settings looked up through `findParam`, a hash table made once at start, instead of the parameter tree's sorted map: about 300 lookups a block; `effectParam` reads the range in place: `getParameterRange` copied it, and a range's small functions could ask for memory on the audio thread); DOMINION 12.2% -> 6.5% (Hard, M/S 15.7% -> 11.1%: its detectors run side by side, samples outer, from flat arrays, instead of each waiting on its last sample); LIMITER true peak 7.2% -> 5.6% (the seven in-between points summed side by side from a table by tap, over a mirrored history); INFERNAL 4.35% -> 3.5-3.9% (phases wrapped by subtracting 1, the bloom's sine skipped while the bloom is exactly 0, its phase still moving); SPECTRE at DRIFT 0 a little (both sides' breath is identical: its filters run once, the right side's set to the left's when DRIFT comes up). On Windows and Linux bit for bit; on Apple-silicon Macs the compiler may fuse a multiply and add differently in the new loops (a rounding difference around -140 dB). Not done (they change the sound): DOMINION in float or at a lower rate for its low detectors, true peak at 4x, fast approximate maths, fast-math / FMA flags, HALO's Newton steps.

## Build, test, check

- **Build:** `cmake -B build && cmake --build build -j8`. On a fresh Linux machine, install the packages listed in `.github/workflows/build.yml` first.
- **Tests (Catch2):** `xvfb-run -a build/tests/ParagonX1Tests`. All must pass. Windows gives a thread 1 MB of stack (Linux 8 MB): keep big data (patterns, tables) on the heap, never inline in `ModMatrix`, `Rig`, `Settings` or the engine (`tests/StackSizeTests.cpp`); before a release run the tests once under `ulimit -s 1024` to catch what Linux hides. New features get tests that check what the feature really does to the sound (levels, frequencies, harmonics), not just that it runs. The synth's test harness (`Player` in `tests/SynthTestUtils.h`) uses `synth::Settings`' defaults, so those must match the parameters' defaults, and seeds the engine's random choices (`seedRandom (1)`), so a test hears the same on every run and computer.
- **pluginval:** `xvfb-run -a <pluginval> --strictness-level 10 --validate-in-process "build/ParagonX1_artefacts/Release/VST3/Paragon PR7-T.vst3"`. Check its exit code. CI runs it too.
- **Screenshots** for checking the UI, via `xvfb-run -a build/tests/ParagonX1Tests "[snapshot]"` with environment variables:
  - `PARAGON_SNAPSHOT` (output path), `PARAGON_SNAPSHOT_WIDTH` (default 1600; 3200 for 2x).
  - `PARAGON_SNAPSHOT_PAGE=mimic` (MIMIC's panel on the PRESETS page, having heard a made G3 with vibrato and breath; `PARAGON_SNAPSHOT_MIMIC=1`: made and refined, its match shown).
  - `PARAGON_SNAPSHOT_PAGE=fx` (the FX page; `PARAGON_SNAPSHOT_RACK=1`: everything at work on "Unison Strings" holding a chord, the sends too; `PARAGON_SNAPSHOT_FX=eq|dominion|comp|limiter|sends|delay|reverb|modfx|mod|mod2|lofi|inserts|insdrive|insglitch|insmod|inslofi`: that view, `PARAGON_SNAPSHOT_BUS=1-4` the inserts' bus; `PARAGON_SNAPSHOT_MATCH=1` with `FX=eq`: EQ Match open), `PARAGON_SNAPSHOT_PAGE=presets|settings|mod|arp` (`PARAGON_SNAPSHOT_ARP=1`: ARP 1 on with a demo pattern, oscillators 1 and 2 routed; `=2`: CHORDS mode with a progression, FIXED NOTE +, a randomizer set up), `PARAGON_SNAPSHOT_PRESET=name` (a factory preset), `PARAGON_SNAPSHOT_OSC=n` (oscillator tab n), `PARAGON_SNAPSHOT_OSCPAGE=voices|shape|fm|fft|draw|travel` (its VOICES, SHAPE, FM, FFT, DRAW or TRAVEL page; `PARAGON_SNAPSHOT_TRAVEL=1` gives oscillator 1 a demo envelope and pattern), `PARAGON_SNAPSHOT_FILTERPAGE=n` (the FILTER panel's SHP n tab), `PARAGON_SNAPSHOT_PAGE=mod` (the MOD page) with `PARAGON_SNAPSHOT_LFO=n`, `PARAGON_SNAPSHOT_ENV=n`, `PARAGON_SNAPSHOT_SHAPE=n`, `PARAGON_SNAPSHOT_PITCH=1` or `PARAGON_SNAPSHOT_STEP=1` (its tabs; STEP 1 with a demo pattern, 2 the sound's own), `PARAGON_SNAPSHOT_FFT=band:dB,...` (a curve on oscillator 1) with `PARAGON_SNAPSHOT_FFT_VIEW=harmonic|bins`, `PARAGON_SNAPSHOT_PARAMS=id=value,...` (e.g. `osc1Wave=4,osc1Table=3,osc1WtIndex=0.3` for a wavetable), `PARAGON_SNAPSHOT_NOTES=1` (a chord held), `PARAGON_SNAPSHOT_MIDI=1` (demo MIDI links, a knob learning, LFO 1 dropped on the cutoff; with `PAGE=settings` the links list).
  - Menus open in their own window, so the editor snapshot can't show one: `PARAGON_SNAPSHOT=menu.png ... "[snapshot-menu]"` draws the FILTER's type menu as the plugin's look-and-feel does.
  - Render, look, crop and zoom before claiming a visual change is right.

## Releases

- Bump `project(ParagonX1 VERSION x.y.z)` in CMakeLists.txt and add a matching `## x.y.z` section at the top of CHANGELOG.md, then push.
- CI builds Windows and Linux and publishes a GitHub Release with the two zips, using that changelog section as the notes. **No macOS build since 0.71.0** (the owner's word: no Mac, and the account's macOS runners never started). It publishes from a push to **any** branch whose version isn't released yet.
- Run the full tests (under `ulimit -s 1024`) **after** the version bump, on exactly what will be pushed: 0.63.0's first build failed CI on the fuzz test when a run made just before the bump had passed.
- After pushing, check the release published (both zips) about 15 minutes later, **and that its `target_commitish` is your commit**. If the version was already taken, CI skips silently and the release you'd link is someone else's build.
- **One chat per plugin at a time.** In October 2026 two chats released PR7-G and PR7-V under the same version numbers: one chat's releases were skipped, and features (and a UI fix) went missing from the builds the owner installed. Before choosing a version, check the repo's tags *and* other branches for unreleased work; if another branch is ahead, merge it first so every release contains everything.
- The owner installs from GitHub Releases on **Windows**.
- **No version tags came over from PR7-K** (they were dropped when the repository was seeded): PR7-T's releases start at `v0.1.0`.

## How the owner likes to work

- **Explain the why**, specifically: what was wrong, what changed, and the trade-offs. Plain language; the owner is a musician, not a DSP engineer.
- **Be honest about limits:**
  - Say when something is our design rather than a copy ("modelled on", "inspired by").
  - Say when a detail is a guess.
  - Say what can't be checked here (audio by ear).
  - Ask for source material, such as manuals, program charts or gear rundowns, rather than invent specifics.
- **No trademarked names or branding** on our models ("Daybreak", "GLITCH", "Obsidian", "Gotham", "AEON"). "Inspired by" in descriptions is fine.
- **Show it:** send a rendered screenshot of any visual change.
- **"merge it"** means: open a PR from the working branch to main and merge it, then reset the working branch to the new main.
- Commit messages explain what and why.
