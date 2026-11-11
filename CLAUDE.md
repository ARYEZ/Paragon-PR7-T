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
2. **THE DECK, the engine** (0.2.0, done): below.
3. **The crossfader** (0.4.0, done): below.
4. **The scratch library** (0.5.0, done): below.
5. **The platter view** (0.3.0 and 0.4.0, done): a drawn turntable and fader to scratch live with the mouse, glowing as it plays.
6. **The sound** (0.6.0, done): below.
7. **Into the FX** (0.7.0, done): the deck's sends, insert bus and the record in presets; below.
8. **Real scratches** (0.8.0, done): the library tuned to measurements of DJs; below.
9. **The second deck** (0.9.0, done): below.

## THE DECK (0.2.0)

- **The engine** (`source/dsp/deck/Deck.*`, our own design): the record's *place* is what moves. The motor turns the platter (START and BRAKE as straight lines, `platter`), the slipmat lets the record follow it (`speed` towards `platter`, 20 ms), a hand moves the record (`Hand::wheel` / `Hand::mouse`: its place follows the hand through two 6 ms smoothing stages, its speed the difference), HOLD stops it (4 ms), BACKSPIN throws it back at SPIN times the base speed (20 ms) and slows it in a straight line to 0 over SPIN TIME (the motor let go), a DROP jumps the needle to a cue (`dropNeedle`: the old place crossfaded out under the new, 4 ms; a hand keeps its hold, moved with it; the motor turned on). Speeds are times the record's natural speed; `Settings::baseSpeed` is the motor's (PITCH, 33/45 = x1.35); a turn is always 1.8 s of the record (`secondsPerTurn`, the groove's 33 1/3 RPM). The pitch wheel's hand grabs when it leaves its centre and lets go 50 ms after it's back there.
- **The cartridge**: the output is the record's sound times `speed / baseSpeed` (signed: backwards inverts it, as a real cartridge's voltage does), capped at +-2 (+6 dB). Normal play is exactly the sample's level; held still is silent (not a stuck sample value).
- **Clean resampling** (`Deck::read`): `Record::levels` holds the sample and copies filtered to 1/2, 1/4, 1/8 of its band (and rate), built by `makeRecord` (`halfBand`: a 127-tap windowed sinc, Kaiser beta 9, cutoff 0.454 of the band so its stopband starts at the new Nyquist and nothing folds back as it's halved; flat to about 0.41; centred so the levels line up). That takes about a second per minute of stereo record, on the message thread when a record is loaded. Reading at rate r (record samples per output sample) uses level floor(log2 r) and a windowed sinc (Kaiser beta 8, 8 zero crossings, tabled at 512 a crossing) whose cutoff is lowered by what's left, so brightness falls smoothly as the speed rises; at r <= 1, level 0 at full band. Each level is padded with 40 samples of silence either side, so the reader never checks bounds (and the cutoff is never so low that the kernel reaches past them: about r 38 at the top level). Read on the record's own samples at normal speed, the kernel's zeros fall on the others: the samples come back exactly (tested).
- **THE HOUSE RECORD** (`houseRecord`, made once, first on the message thread in the processor's constructor): 8 s at 48 kHz, an "ahh" (additive voice through three formants), a horn stab (additive saws, a filter blat), a break at 95 BPM and a 1 kHz tone (6.0 to 7.6 s), each part brought to a peak of 0.7.
- **In the processor**: DECK ON (`deckOn`, off by default) takes the deck's 12 keys (`deckKeys`: octave index i from note 12 (i + 1); default C1 = 24, middle C = C4 as the keyboard shows) and the pitch wheel out of the synth's MIDI (`processBlock`: `deckMidi` / `synthMidi`, both sized in `prepareToPlay`, so nothing is allocated on the audio thread). The deck is added into each piece of `processChunk` right after the engine writes it, so it goes through the rack, OUTPUT and the LIMITER (not the sends or insert buses yet). The record is handed over as MIMIC's recording is: `deckRecordForAudio` a plain pointer, a replaced record kept 2 s (`retiredDeckRecords`, cleared in `updateWavetables`). Saved with the project as a `DeckRecord` child (its file; none: the house record); presets don't hold it. A missing or unreadable file is kept named (so the project keeps asking for it), `getDeckRecordProblem` says why, the house record plays meanwhile. Loads WAV, AIFF, FLAC, Ogg (JUCE's basic formats; MP3 is off in the build), up to 3 minutes (about 70 MB as floats, plus a third for the filtered copies).
- **Parameters** (`DeckParameters.cpp`): version hint **44**. Hints must only ever rise for hosts, so PR7-T carries on from PR7-K's 43 though its own version restarted at 0.1.0. The cue IDs are a `const char*` array (`ParamIDs::deckCues`): `param()` is called on the audio thread, and building a `juce::String` there would allocate.
- **The page** (`DeckPage.*`, tab DECK beside SYNTH; the seven tabs share the old six's group evenly): the platter (`DeckPlatter`: turns with the record, the arm moves in through it, dragging round it is `setDeckMouseHand`), the strip (`DeckStrip`: the record's peaks, the needle, the cues), the keys as buttons (they press the deck's notes on the keyboard state, so they act exactly as MIDI; off while the deck is off, since they'd play the synth), the knobs. Snapshot: `PARAGON_SNAPSHOT_PAGE=deck`.
- **Tests** (`tests/DeckTests.cpp`, `[deck]`): sample-for-sample at normal speed; pitch fader and 45 by measured frequency; START / BRAKE as straight lines; BACKSPIN's pitch and level; HOLD; the wheel's reach and letting go; aliasing (15 kHz at 2x and 3.3x more than 60 dB down, tones that fit within 3%); the drop's place, no click, the thump; the house record; the plugin's key split; the record saved with the project; the page's keys and platter. Not checked here: how it sounds by ear.

## THE TURNTABLE in the "Tron" look (0.3.0)

- **The owner's ask:** "add the Tron aesthetic that we used for the PR7-I, the PR7-O and the PR7-D". In those, "Tron" is the instrument picture's look: our own model rendered in Blender 4.0, gloss black, neon pink `ff2d6e` / red `ff2a2e` / orange `ff6a24`, a `c0102c` grid floor, Filmic, fog glow, every hue pulled into 318-42 degrees (PR7-D's kit view, PR7-I's GuitarView, PR7-O's OrchestraView). PR7-O also took PR7-I's rack for its knobs at the owner's later ask; PR7-T did in 0.4.0 (below).
- **`resources/deck/`**: `turntable3d.py` (PR7-I's `guitar3d.py` materials, `neon` by world X like PR7-D's kit, floor, lights, compositing; our own direct-drive deck; README there), `process_deck.py` (PR7-I's `family_hues`; the turning layers cut to their alpha's box; `deck.json`). The camera is **straight down and orthographic** (the others tilt theirs a few degrees): the page turns the platter layer and swings the arm layer as flat pictures, and only a top-down view lets them line up with the deck under them at any angle. The platter's vinyl is fairly matt (a highlight baked into it would turn with it); `DeckPlatter` lays a still sheen over it instead. The base pass includes the platter (its rim's glow on the floor and plinth) under the turning one; the arm is left out of it (it moves).
- **`DeckPlatter`** (`DeckPage.cpp`): `deckPictures()` (the binary data and `deck.json`, once), the picture fitted and centred in the component (`pictureArea`, `RectanglePlacement::centred`), `prepare` shrinking all four pictures once to the screen's pixels (PR7-D 0.15.1's lesson; remade only when the size or scale changes), the deck copied with its corner on a whole screen pixel, `deck-lit` faded in by the record's speed while the deck is on, the platter rotated about its centre by the record's turns, a still sheen, the arm rotated about its pivot by `armAngle` (`needleAt`: where the needle's circle meets the arm's reach, the branch found once from the rendered stylus; the needle's radius from `grooveStart` to `grooveEnd` with the record's progress). Repaints only when the turn moves the record's edge a quarter pixel, the light by 1%, or the arm by 0.0005 rad. The hand grabs only within 1.1 record radii of the spindle.
- Tests: `[colours]` (every coloured pixel of the four pictures in 318-42), the arm's angle and the picture changing as it plays, the page's hand on the rendered platter's centre.

## THE CROSSFADER and THE RACK (0.4.0)

- **The owner's ask:** "Go ahead with step 3 adding in the crossfader and yes please let's bring in the pr7i rack style panel for all the knobs like we've done with the other instruments".
- **The crossfader** (`Deck.*`): `faderGain (fader, curve, hamster)`: x the fader's openness (HAMSTER: 1 - x), open fully at `1 + (0.01 - 1) * curve` of the travel (CURVE 0: the whole travel, a quarter sine, -3 dB at the middle; 1: 1%), a quarter sine up to there. In `process`, after LEVEL, on everything the deck makes (the needle too: the mixer's channel): `faderNow` moves to its target in straight 1 ms lines (sharp, no click: tested). **CUT** (key 6, F#) flips the openness while held (x to 1 - x: open to cut, or a closed fader tapped open); **TRANSFORM** (key 8, G#) flips it for the second half of each cycle of `transformBeats[transformRate]` (1/8 0.5, 1/16 0.25, 1/32 0.125, 1/8T 1/3, 1/16T 1/6 beats), from `Settings::ppq` (the song's place at the piece's start; `processChunk` moves it on per 512-sample piece) or, the song stopped, from the press (`transformBeat`). Keys 10 and 11 (A#, B) are still free for stage 4. `getFaderGain` and `takePeak` (the deck's own peak after the fader, for the rack's meter: the processor's output meter is already read by the OUTPUT column, and `takePeak` empties it). Parameters (`DeckParameters.cpp`, version hint **45**): `deckFader` (default 1: open, so older projects are unchanged), `deckCurve` (default 1: a scratch cut), `deckHamster`, `deckTransform` (choice, `deck::transformNames`, only append).
- **THE RACK** (`ui/DeckRack.*`): PR7-I's `GuitarRack` with the same parts (`PedalBody::paintUnit` / `paintBadge`, `LedDisplay`, stamped lettering, printed red brackets and lines, `drawNeonBar`), always pearl black (`Finish::paintAs`, ported from PR7-I into `Finish.*`). Badge "TURNTABLE INSTRUMENT" + PAGE keys DECK / FADER / CUES (`setPage`, `onPageChanged`); the screen (`Screen`: caption, record, state, line1 = position, line2 = keys or the record's problem) over ON / LOAD / HOUSE (`keyCell`; RECORD bracket); the page: `knobCell` (8 in a row; FADER and CUES use the first 4), `faderScreenArea` (FADER: the gain as 24 LED bars and the keys), `menuCell` + `keyMapArea` (CUES: KEYS on a screen, the key map in two columns); DECK OUT: `meterArea` (PR7-I's ladder, the deck's peak) and `sideKnobCell` (LEVEL); no OUTPUT knob (the window's OUTPUT column stays, as on PR7-O). `preferredHeight` 260.
- **`ParamControl`**: PR7-I's `setRack` (the name stamped above in the rack's ink, the pedal knob, the value on an orange screen; menus orange on the rack's screens) and `knobForChoices` (RPM and TRANSFORM as knobs), keeping PR7-K's modulation ring, MIDI learn and drops. New here: a switch on a rack becomes one of its small keys (a `TextButton` with `pedalButtonProperty`, toggling; swapped and attached again in `setRack`), as HAMSTER. `DeckPage` calls `setRack` inside `Finish::paintAs (pearlBlack)` so the lettering is the black rack's ink whatever finish is chosen.
- **`DeckPage`**: the rack across the top (its controls are the rack's children, shown by page in `showRackPage`); the turntable below at its shape; beside it HIDE RACK / SHOW RACK (`setRackShown`: the rack hidden, a painted header names the record and the turntable gets the height, as PR7-I 0.6.3), the strip, the ten keys (one row where each gets 90 px, else two rows of five), **`DeckCrossfader`** (a `juce::Slider` on `deckFader` drawing a mixer's fader: its marks, CUT / DECK ends (swapped by HAMSTER), the open travel lit by the deck's fader gain, the cap), the legend. Snapshots: `PARAGON_SNAPSHOT_DECKRACK=fader|cues|hidden`.
- Tests: `[fader]` (the curve's values and that it never closes as it opens, open = 0.2.0's output sample for sample, cut silent within 1 ms with no click, CUT both ways, TRANSFORM's openings at four rates and its timing with and without the song, the parameters in the plugin, the page's fader), `[rack]` (every deck parameter on the rack in rack style, each page's controls inside the unit without overlapping, ON, the screen), HIDE RACK (the turntable 1.4 times bigger).

## THE SCRATCH LIBRARY (0.5.0)

- **The owner's ask:** "Go for step 4" (the plan: each technique as a hand-motion curve plus fader cuts, synced to tempo; a key plays one, velocity its intensity; the mod wheel can take the record by hand).
- **The techniques** (`Deck.*`): `scratchNames` (12: BABY, FORWARD, BACKWARD, CHIRP, STAB, DRAG, TEAR, SCRIBBLE, TRANSFORMER, FLARE, 2-CLICK FLARE, CRAB), `scratchBeats` (0.5 / 1 / 2 beats), `scratchAt (technique, phase)` -> `{ hand, open }`: the hand 0 to 1 of the stroke (forward; every shape starts and ends at 0, so a scratch leaves the needle where it took it), the fader open or cut. Our own shapes from how the techniques are generally described (the stroke `(1 - cos 2 pi p) / 2`; cuts by where in each half-stroke it is); the owner can be asked for references to tune them. Only two of the deck's 12 keys were free, so the library has **its own 12-key block** (`firstScratchKey`, the deck's own keys win where they overlap).
- **Playing one**: a note in the block grabs the record with `Hand::scratch` (the hand's two 6 ms smoothing stages, as the wheel's: a technique's corners never click), `scratchStroke` = `scratchReach * (0.4 + 0.6 * velocity)` turns, `scratchPhase` on in beats at `Settings::bpm` times `scratchRates[scratchRate]` (1/2x, 1x, 2x); at its end, again while the key's held (`scratchHeld`), else the hand lets go (the motor takes the record on). The mouse's hand wins (a scratch key does nothing while the mouse holds the record); a scratch takes the record from the pitch wheel's hand. Its cuts: `scratchOpen` false sets the crossfader's target to 0 (so CURVE and the 1 ms ramps apply). Not quantised to the beat: it starts on the key.
- **MOD HAND** (`Hand::modWheel`, `Settings::modHand`): CC 1 moves the record by `(value - value at grab) * 2 * reachTurns` turns; still 150 ms, the hand lets go (and grabs again from there on the next move). The processor gives CC 1 to the deck only while MOD HAND is on (else it stays the synth's mod wheel).
- **Parameters** (`DeckParameters.cpp`, version hint **46**): `deckScratchKeys` (choice: OFF, C0-C7; default C2 = note 36; `deckScratchKeyNames`, only append), `deckScratchReach` (0.05 to 1 turn, 0.25), `deckScratchRate` (choice, `scratchRateNames`), `deckModHand` (off). `processBlock` routes the block's notes (and CC 1 with MOD HAND) to `deckMidi`.
- **The SCRATCH page** (`DeckRack::Page::scratch`, the fourth PAGE key; `pageKeyCell` now quarters the badge column): REACH, RATE (a knob for its choices), MOD HAND (a rack key) in `knobCell` 0-2, SCRATCH KEYS on `menuCell`'s screen, the techniques in two columns of six on `keyMapArea` (`setScratchList` with the one playing lit from `Deck::getScratch`; `scratchAt` finds the line under a point; `mouseDown` / `mouseUp` call `onScratchPressed`, which `DeckPage` turns into the technique's note on the keyboard state, as MIDI). Snapshot `PARAGON_SNAPSHOT_DECKRACK=scratch`.
- Tests: `[scratch]` (every shape starts and ends at 0 within 0..1, the cuts per technique, TEAR's stop, SCRIBBLE's size; BABY's stroke length and timing at 120 and 60 BPM, forward then back, back where it started; velocity's 40%; held repeats, let go finishes; FORWARD heard on the push only; MOD HAND on and off; the plugin's block and OFF; the page's click).

## THE SOUND (0.6.0)

- **The owner's ask:** "Go for step 6" (the plan: cartridge and mixer EQ, the mixer's filter and kill switches, lo-fi).
- **`dsp/deck/DeckMixer.*`** (run by `Deck::process` on each sample after LEVEL, before the crossfader, so it's the mixer's channel; `set` once a block from `Settings::mixer`): **CART** (`cartNames`: CLEAN, SCRATCH, WARM; a low shelf, a peak, a high shelf a side, RBJ biquads in double; our voicings: SCRATCH +3 dB at 3.2 kHz Q 0.9, -4 dB shelf over 12 kHz, -1.5 dB under 80 Hz; WARM -3.5 dB over 6 kHz, +2 dB under 120 Hz), the **isolator** (LR4 = two Butterworth biquads at 250 Hz and 2.5 kHz; the low band through a 2nd-order allpass at 2.5 kHz, **Q 0.7071**, so the three bands sum flat within 0.0002 dB: checked numerically, Q 0.5 would be 0.018 dB off; band gains -40 dB (= off) to +6, a KILL or the floor = 0, 5 ms one-pole), the **FILTER** (TPT state-variable filter, `filterHz`: left 20 kHz to 60 Hz low-pass, right 20 Hz to 8 kHz high-pass, the knob smoothed 20 ms, coefficients every 16 samples; RES `k = 1.414 - 1.29 res`; across the centre it fades out, changes type silent, fades in on the new side). Each section **bypassed exactly while neutral** (`process` returns untouched), crossfaded 10 ms in and out (its filters cleared when it starts from fully out). Tests check neutral = untouched sample for sample.
- **WEAR** (`Deck`, `Settings::wear`): surface noise added to the record's sound before the cartridge: hiss a side (white noise through a one-pole whose cutoff is 6 kHz times the speed, level `0.03 wear speed`) and crackle (a random click, 25 a second at full wear and speed, 0.3 ms decay), all scaled by how fast the record moves (`min (2, |speed|)`), so silent held, louder scratched; the warp multiplies the motor-driven position step by `1 + 0.004 wear sin (2 pi position / turn)` (a place on the record). `wearRandom` is seeded by `seedRandom`.
- **Parameters** (`DeckParameters.cpp`, version hint **47**): `deckCart` (choice), `deckEq[3]` (-40 to +6 dB, skewed so the middle is -6 dB; "OFF" at the floor), `deckKill[3]`, `deckFilter` (-1 to 1, "LP 2.6 kHz" / "HP ..."), `deckFilterRes` (0.3), `deckWear` ("NEW" at 0).
- **The MIXER page** (`DeckRack::Page::mixer`, the fifth PAGE key; `pageKeyCell` fifths the badge column, 34 px keys): CART (a knob for its choices), LOW, MID, HIGH in `knobCell` 0-3, the KILLs stacked in `killCell` 0-2 (knobCell 4 in thirds; `ParamControl` centres a rack switch's key in a short cell), FILTER, RES, WEAR in 5-7. Snapshot `PARAGON_SNAPSHOT_DECKRACK=mixer`.
- **Fixed** (found by the MIXER page's screenshot): `ParamControl::setRack` swapped a switch for a rack key and called `attach (parameterId)`, which returns at once for the parameter it already has, so the key was never attached (HAMSTER on 0.4.0-0.5.0 neither showed nor switched). It's attached directly now; tested by pressing it.
- **A lesson about builds:** editing a header while a build compiles can leave a stale object that ninja thinks is current (it was written after the edit, from the old text). The test then reads the class at old offsets: here a 0x0 platter and a crash in a test that passed alone after recompiling. Don't edit while building; before a release, build from clean (`--clean-first`) or bump the version (which recompiles everything).
- Tests: `[sound]` (neutral untouched; the isolator flat at 0 dB, the kills more than 40 dB at 60 Hz and 10 kHz and 30 dB at 800 Hz, -6 / +6 dB bands; the FILTER's ends more than 30 dB down at 1 kHz, RES's peak more than 10 dB, no click across the centre; CART's voicings; WEAR silent held, louder faster, the warp's 0.4% measured by the needle's speed (zero crossings would count the crackle); a KILL's ramp; the parameters in the plugin); the rack's switches lit and switching.

## INTO THE FX (0.7.0)

- **The owner's ask:** "Send the deck through the FX sends an insert buses and save the record and presets".
- **Routing** (`processChunk`): the deck renders into `deckScratch` (2 x 512, sized in `prepareToPlay`), then is added
  to the mix or, with `deckInsert` (choice `insertBusNames`: NONE, A-D) to that bus's buffer (the bus marked `used`,
  as an oscillator's is, so the engine clears and runs it); `deckSendDelay` / `deckSendReverb` add it into
  `rack->sends`, after the fader, each only while `delayOn` / `reverbOn` (an off send's buffer would come back dry).
  Version hint **48**, all default off.
- **The record in presets** (`Rig::deckRecord`, a `std::optional<juce::String>`, saved as a `DeckRecord` element with
  `file`): `captureRig` fills it (`""` = the house record); `applyRig` applies it only when present (older and factory
  presets leave the record alone), reloading only when it names another file or the current one had a problem.
  `deckRecordChangedByUser` (LOAD, HOUSE) marks the rig modified and makes an undo checkpoint; `Snapshot::operator==`
  compares the record. The sample isn't embedded: the path is kept, as in projects.
- **The ROUTING page** (`DeckRack::Page::routing`, sixth PAGE key; `pageKeyCell` two rows of three, 44 x 26):
  TO DELAY, TO REVERB, INSERT (a knob for its choices) in `knobCell` 0-2, the info screen (`setRoutingInfo`,
  `paintRoutingScreen` in `faderScreenArea`) saying which effects are off. Snapshot `PARAGON_SNAPSHOT_DECKRACK=routing`.
- Tests: `[routing]` (bus A's DRIVE adds harmonics to the 1 kHz tone at a steady level; the delay's echo rings on after
  a fader cut; sends at 0 are sample-identical to 0.6.0), `[presets]` (the Rig round trip, save and load, a factory
  preset leaves the record, a missing file kept named, LOAD undone).

## REAL SCRATCHES (0.8.0)

- **The owner's ask:** "Tune the scratches to real ones and also add the second deck" (the deck: 0.9.0).
- **Sources** (the KTH PDFs and Wikipedia couldn't be fetched from the session; what was used came through web search):
  K. F. Hansen (KTH), "The basics of scratching" (2002), Hansen and Bresin "Analysis of a genuine scratch performance"
  (2003), Hansen, Fabiani and Bresin (2009): a baby cycle about 260 ms; backward strokes 11% faster; most movements
  slower than the record's nominal speed; chirps "on every movement forward, the fader closes fast after the start, and
  backwards only the last bit of the sound is cut in"; fader openings from about 10 ms. Tutorials (DJ Times "Itching
  to Scratch", School of Scratch, DJ wikis): transformer and crab rest closed, tapped open; flares start open, named by
  their clicks; tear a stop in one direction; scribble a tensed forearm, no fader.
- **Changes** (`scratchAt`): `scratchPush` 0.53 (every stroke-based technique pushes 53% of its time); CHIRP open at
  `half < 0.35` of the push and `half > 0.65` of the pull; TRANSFORMER open 35% of each 1/32; CRAB 40% taps; SCRIBBLE 4
  strokes per half beat; `Settings::scratchReach` (and the parameter's default) 0.15 (projects keep their saved value).
- **Still our guesses:** the ease shape of a push, a flare's click places and lengths. Ask the owner for references
  (songs with timestamps, videos) to match more closely.
- Tests: `[scratch]` (the push's 53%, CHIRP's openings and its sound in the deck, TRANSFORMER and CRAB open under 40%,
  SCRIBBLE's four strokes).

## THE SECOND DECK (0.9.0)

- **The owner's ask:** "Tune the scratches to real ones and also add the second deck" (the scratches: 0.8.0).
- **Parameters** (`Parameters.h`, `DeckParameters.cpp`, version hint **49**): `ParamIDs::DeckIds` (every per-deck ID) and
  `deckIds[numDecks]`: deck A's are the old IDs (their hints unchanged, in their old order), DECK B's "deckB..."
  (`deckBOn` off; keys C3 = `deckBKeysDefault` 3, scratch keys C4 = `deckBScratchKeysDefault` 5). `addDeck` makes a
  deck's list; the mixer's shared ones (`deckFader`, `deckCurve`, `deckHamster`, `deckTransform`, `deckModHand`) only
  on deck A's, plus `deckHand` (choice A/B: the deck the pitch wheel and MOD HAND move; `handDeck()` picks the other if
  that one's off and it's on).
- **The A/B fader:** `deckFader` 0 = A's end, 1 = B's, default 0.5. `getDeckSettings (d)` gives each deck its side as
  its own `Settings::fader` (A `1 - x`, B `x`; the engine's "1 open" unchanged, HAMSTER in the engine).
  **Migration:** a state or preset without `deckBOn` (from before 0.9.0) has `deckFader` turned round (`1 - v`) in
  `setStateInformation` and `Rig::fromXml`, so deck A sounds as it did. A MIDI-learned fader now runs the other way
  (said in the changelog). **CUT / TRANSFORM** (`Deck::process`): `faderFlipped = faderSet >= 0.5 ? 0 : 1` (a snap to
  the other edge; mirroring the place, as before, does nothing in the middle). A technique's cuts: its own deck only.
- **The processor:** `decks[2]`, `deckMidi[2]`, `deckSides[2]` (record, `forAudio`, file, problem); every deck call
  takes `int d = 0` (A when left out, so 0.8.0's callers and tests are unchanged). `processBlock` routes a note to the
  first on deck whose 12 keys or scratch block hold it (A wins overlaps), the wheels to `handDeck()`. `processChunk`
  renders each on deck into `deckScratch` and routes it (its own bus and sends). States: `DeckRecord` and
  `DeckRecordB` children; presets: `Rig::deckRecord` and `deckRecordB` (unset: left alone).
- **The page:** `DeckPlatter (p, d)` and `DeckStrip (p, d)`, two of each; A left, B right, each at most 30% of the
  width (32% with the rack hidden: more and the keys between them truncate), between them the strips (capped 130 px),
  the shown deck's ten keys (two rows of five), the legend, `DeckCrossfader` (A/B ends lit by each deck's gain).
  `DeckPage::sets[2]` (each deck's rack controls, ON key), shown by `selected` (`selectDeck`: the rack's A / B keys,
  `deckKeyCell`, or `DeckPlatter::onGrab`); shared `faderControls` and `handControls` (MOD HAND, HAND on SCRATCH's
  `knobCell` 2-3). `DeckRack::underScreenCell` puts five keys under the screen (A, B, ON, LOAD, HOUSE).
- **The mixer in the Tron look (0.9.1;** the owner: "can you give the graphics between the decks the Tron style
  treatment too?"**):** drawn in `DeckPage.cpp`'s `tron` namespace from `turntable3d.py`'s recipe, not rendered:
  `tron::at` / `across` (ff2d6e, ff2a2e, ff6a24 left to right: the renders' swatch, not `Finish::neonGradient`'s
  darker rack one), `tube` (a neon line: three glow widths, the tube, a white-hot core), `gloss`, `tron::Key` (the ten
  keys: neon edge by `t`, their place). `DeckPage::paint`: the floor (0xff161212, the pictures' edge measured), a red
  glow round `mixerPlinth`, the grid (c0102c at 32%, every `DeckPlatter::gridStep` 49.95 picture px, rows from
  `gridFirstRow` 39.6: measured from deck.jpg), the plinth with its neon edge. `resized` moves DECK B left by the grid's
  remainder so its picture's grid lines up with A's, and centres the mixer's parts in the height left.
- Tests: `[twodecks]` (B's keys play B, A's A, both twice as loud, B off gives its keys back; the fader's ends and
  middle, B's CUT only B, a tap from the cut end; HAND; B's record with the project and in presets, the old project's
  and old preset's fader turned round, deck A heard; the page's two turntables, grabbing B shows and plays B).

## The project

- An AGPLv3 **JUCE VST3** instrument, a turntable (THE DECK) with PR7-K's synthesizer, in the PR7 family's look, sharing the family's code.
- **Code layout:** THE DECK in `source/dsp/deck/` (its page `source/ui/DeckPage.*`, its parameters `source/DeckParameters.cpp`), the synth in `source/dsp/synth/`, PR7-E's effects (for stage 7) in `source/dsp/effects/`, `dsp/eq/`, `dsp/dominion/`; UI in `source/ui/`, parameters in `source/Parameters.*`, presets ("rigs" in the code, as across the family) in `source/rigs/`, tests in `tests/`.
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
