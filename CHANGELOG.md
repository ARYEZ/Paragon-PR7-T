# Changelog

Each version's notes appear on its GitHub Release page.

## 0.8.0

**The scratch library tuned to how DJs really scratch.**

0.5.0's twelve techniques were our own shapes, from general descriptions. This version checks them against
measurements of real DJs: Kjetil Falkenberg Hansen's research at KTH (Stockholm), which tracked DJs' record and
crossfader movements with sensors ("The basics of scratching", 2002; "Analysis of a genuine scratch performance",
2003; with Fabiani and Bresin, 2009), and against how turntablists describe each technique. Where ours differed,
they're changed:

- **CHIRP was wrong; it's fixed.** Hansen: "on every movement forward, the fader closes fast after the start, and
  backwards only the last bit of the sound is cut in." So a chirp is heard at the *start of the push* and the *end of
  the pull*, with the fader shut round the far turn. 0.5.0 cut the end of both strokes instead.
- **TRANSFORMER and CRAB: the fader rests closed now.** In both, a DJ's fader sits shut and is tapped open (a
  crab's thumb springs it back shut after each finger's tap), so you hear short bursts. 0.5.0 had the fader open
  half the time, which sounded more like a gated chop.
- **The pull is quicker than the push.** Hansen measured DJs' backward strokes about 11% faster than their forward
  ones, so every stroke now pushes for 53% of its time and pulls back in 47%.
- **SCRIBBLE is slower**: 16 tiny strokes a second at 120 BPM (a tensed forearm's tremor), not 32.
- **REACH starts at 0.15 of a turn** (was 0.25). A baby scratch cycle measured about 260 ms, and DJs' records mostly
  moved slower than normal playing speed. A quarter turn in that time made the scratches squeal higher than real ones.
  Your saved projects keep the REACH they had; turn it up for the old, wilder sound.
- BABY, FORWARD, BACKWARD, STAB, DRAG, TEAR, FLARE and 2-CLICK FLARE already matched the descriptions; they only
  get the quicker pull.

What's still our guess: the exact shape of a hand's push (we use a smooth ease in and out), where a flare's click
falls, and how long each click lasts. If you have songs or videos with scratches you want PR7-T to nail, send them
(with timestamps) and they can be matched more closely.

## 0.7.0

**THE DECK into the FX: its own sends and insert bus, and the record saved in your presets.**

Until now the deck went straight into the main mix (through the rack's effects, OUTPUT and the LIMITER) but couldn't
reach the delay, the reverb or the insert buses, and a preset didn't know which record was on. A sixth rack page,
**ROUTING**, fixes the first; presets now fix the second. The PAGE keys under the badge are two rows of three now, so
six fit.

- **TO DELAY** and **TO REVERB**: the deck's own send to the FX page's DELAY and REVERB, as a mixer channel's aux
  sends. They take the deck **after its crossfader**, so a cut silences the dry deck but the echoes and the tail ring
  on (tested: after a cut, the delay's repeats are still heard). A send is heard only while its effect is on (FX >
  SENDS); the ROUTING page's screen says so when one is off.
- **INSERT**: NONE, or bus **A-D**: the deck plays into that insert bus instead of the main mix, so its effects
  (DRIVE, GLITCH, MOD, LO-FI) are on the deck, as they are on any oscillator sent there (tested: bus A's DRIVE adds its
  harmonics to the house record's 1 kHz tone, at a steady level).
- All three start at off, so your projects sound exactly as before (tested: sample for sample).
- **The record in your presets**: saving a preset now saves which record is on the deck (the file's place, or the
  house record), and loading it puts that record back. Changing the record (LOAD, HOUSE) marks the preset as changed
  (the `*`), and can be undone. Presets from before 0.7.0, and the factory presets, leave the record alone, so
  browsing them won't swap your record out. As with projects, the sample itself isn't copied into the preset: it's
  found where it was. If it has moved, the deck says it's missing and plays the house record meanwhile.

## 0.6.0

**THE SOUND: the cartridge, a DJ mixer's channel, and a worn record.**

What's between the needle and your speakers on a DJ's setup, on a fifth rack page, **MIXER**. All of it starts neutral,
so your projects sound exactly as they did until you turn something.

- **CART**, the cartridge's voicing: **CLEAN** (flat), **SCRATCH** (the presence lift and softer top scratch
  cartridges are known for: +3 dB around 3.2 kHz, the very top a little softer) and **WARM** (rounder highs, a little
  more low end). Our own voicings, not copies of anyone's cartridge.
- **LOW, MID, HIGH** with **KILL LOW, KILL MID, KILL HIGH**: the mixer channel's EQ as an *isolator*, the kind battle
  mixers have. The sound is split into three bands (under 250 Hz, 250 Hz to 2.5 kHz, over 2.5 kHz) that add back up
  perfectly flat, and each band can be turned up to +6 dB, turned down, or killed outright (measured: a kill takes its
  band down by more than 40 dB, the rest untouched). Killing the bass on a drop, or the highs on a build, is clean.
- **FILTER** and **RES**: the one-knob DJ filter. Left of centre a low-pass sweeping down to a muffled 60 Hz, right a
  high-pass up to a thin 8 kHz, the centre off. **RES** gives it the peak DJ filters sing with. Swept straight across
  the centre it fades over instead of clicking.
- **WEAR**, a worn record: surface hiss and crackle that are *in the groove*, so they move with the record: louder
  and brighter as you scratch it faster, silent when you hold it still. And a warp: the pitch wobbling a little (up
  to 0.4% either way) once each turn.
- Every switch and change is smoothed (a kill is a 5 ms fade, a section switching in or out a 10 ms crossfade), so
  nothing clicks.
- **Fixed: HAMSTER's key didn't work.** On the rack it showed off and pressing it didn't change HAMSTER (0.4.0 and
  0.5.0); the knob-free switches on the rack (HAMSTER, the KILLs) now light while they're on and switch when pressed.

## 0.5.0

**THE SCRATCH LIBRARY: a scratch on every key, in time with your song.**

- **Twelve techniques, one per key**, on a block of their own (**SCRATCH KEYS**: C2 by default, the octave above the
  deck's own keys; OFF gives the keys back to the synth): **BABY** (C), **FORWARD** (C#), **BACKWARD** (D), **CHIRP**
  (D#), **STAB** (E), **DRAG** (F), **TEAR** (F#), **SCRIBBLE** (G), **TRANSFORMER** (G#), **FLARE** (A),
  **2-CLICK FLARE** (A#), **CRAB** (B).
- Each is a hand on the record and the crossfader together: the record pushed and pulled (BABY forward and back;
  TEAR back in two moves with a stop; SCRIBBLE tiny tense strokes; DRAG a slow push) while the fader cuts it (FORWARD
  heard on the push, BACKWARD on the pull, CHIRP each stroke's tail cut, FLARE one click shut in each stroke, the
  2-CLICK FLARE two, CRAB four quick taps, TRANSFORMER chopping 1/32s). Our own shapes, from how these scratches are
  generally described; tell me if one should feel different.
- **In time with your song**: each takes a set number of beats at the song's tempo (BABY, FORWARD, BACKWARD, CHIRP,
  STAB and SCRIBBLE half a beat; DRAG, TEAR, the FLAREs and CRAB a beat; TRANSFORMER two). **RATE** plays them at half,
  normal or double speed.
- **How hard you hit the key** sets the stroke: soft a short one, hard the full **REACH** (40% to 100% of it).
- **Hold the key and it repeats**; let go and it finishes the stroke it's on, then the motor takes the record on (or,
  with the motor stopped, it stays where the scratch left it: back where it started).
- Its cuts go through the crossfader as yours do (CURVE, and the 1 ms ramp that never clicks).
- **MOD HAND** (off by default, since the synth uses the mod wheel): the mod wheel as a hand on the record. Unlike the
  pitch wheel it stays where you leave it; the whole wheel moves the record twice REACH. Still for a moment, the hand
  lets go, and takes the record again from there when you move it.
- **A SCRATCH page on the rack** (the fourth PAGE key): REACH, RATE, MOD HAND, SCRATCH KEYS, and a screen of the
  techniques with their keys, the one playing lit. **Click a technique to play it** (held, it repeats). The record's
  screen names the technique while it plays.
- **Fixed: stuck keys.** A held key (HOLD, CUT, TRANSFORM, a technique) used to stick if you moved its keys (KEYS,
  SCRATCH KEYS) or turned the deck off while holding it, as its note-off then went to the synth (a scratch would
  repeat for ever). The deck now lets go of them.

## 0.4.0

**THE CROSSFADER, and THE DECK's controls on a rack unit like PR7-I's.**

- **The crossfader** (stage 3): half of every scratch. The deck's channel is cut in and out the way a DJ mixer's is.
  - **FADER**: how open it is. Open by default, so your 0.2.0 and 0.3.0 projects sound exactly as they did.
  - **CURVE**: from **SMOOTH**, a mixing fade (-3 dB at the middle), to **CUT**, a scratch mixer's cut: fully open
    after the first 1% of the fader's travel. It starts on CUT.
  - **HAMSTER**: turns the fader round, as many scratch DJs set theirs.
  - **Two new keys**: **CUT** (F#) flips the fader while you hold it: it cuts the sound, or with the fader closed it
    taps it open (with the pitch wheel: chirps and stabs). **TRANSFORM** (G#) chops the sound on and off in time with
    your song while you hold it, at **TRANSFORM**'s rate (1/8, 1/16, 1/32, 1/8T, 1/16T): the transformer scratch. With
    the song stopped it keeps its own time, from when you pressed it.
  - Every move is a 1 ms ramp: as sharp as a real cut sounds, without a click.
  - **A crossfader on the page**, under the record, to drag with the mouse, its open travel lit as the deck is heard.
    Or right-click FADER on the rack to MIDI-learn a controller's fader.
- **The rack**: all of THE DECK's controls are now on one of the family's rack units across the top of the page, the
  same as PR7-I's guitar rack and PR7-O's: the pearl black unit with its red light, PARAGON PR7-T, the big knobs with
  their values on small orange screens, the small neon keys, the LED screens. Three pages, from the PAGE keys:
  - **DECK**: PITCH, RPM, START, BRAKE, SPIN, SPIN TIME, REACH, NEEDLE.
  - **FADER**: FADER, CURVE, TRANSFORM, HAMSTER, and a screen of the crossfader at work.
  - **CUES**: CUE 1-4, KEYS, and a screen listing the deck's keys.
  - The screen shows the record, whether it's playing, stopped or going backwards, and where the needle is; under it
    **ON**, **LOAD** and **HOUSE**. **DECK OUT** has the deck's own meter (after the crossfader: you see the cuts) and
    its **LEVEL**. The rack is pearl black whatever finish you choose, as PR7-I's is.
- **HIDE RACK / SHOW RACK**, as on PR7-I: hide the rack once your settings are made and the turntable gets the page's
  height.
- The keys under the record now include CUT and TRANSFORM (in two rows when the rack's hidden, so their names fit).

## 0.3.0

**THE TURNTABLE in the family's "Tron" look.**

- **The DECK page's turntable is now our own 3D deck, in the same "Tron" look as PR7-D's kit, PR7-I's guitar and
  PR7-O's strings**: a gloss black plinth with a glowing edge, a black chrome platter with a neon rim and strobe dots,
  black vinyl with a glowing label ring and "PR7-T" on the label, a tonearm with a neon strip, the pitch fader,
  START / STOP and 33 / 45, on a gloss floor with the red grid. The neon runs pink at the left through red to orange
  at the right, as the kit's does. Modelled and rendered in Blender with the same materials, lights and glow as the
  guitar's. Our own design: no maker's layout, name or logo.
- **It moves**: the platter turns as the record does (forwards, backwards, held, spun back), the arm swings in as the
  needle plays through the record, and the neon brightens as the record turns (dimming as it stops).
- **Grab the record** as before: drag round it with the mouse to scratch. Now only the record grabs it (not the
  plinth around it).
- The turntable takes about half the page at its own shape; the knobs beside it are a little narrower.
- Light on the CPU: the pictures are shrunk to your screen's size once (not on every frame, the lesson of PR7-D's kit
  view), and the deck is only redrawn when something on it moves: a stopped record costs nothing.

## 0.2.0

**THE DECK: a turntable to scratch, hold, spin back and drop the needle on.**

- **A new DECK tab**, beside SYNTH: a record on a platter, played at whatever speed and direction the motor and your
  hand give it, forwards or backwards, as nu-metal's DJs played theirs. Our own design.
- **Off by default**, so every preset and project plays as before. Turn it **ON** and the deck takes 12 keys (from C1,
  the on-screen keyboard's lowest octave; **KEYS** moves them) and the pitch wheel away from the synth:
  - **PLAY** (C), **STOP** (C#), **BACKSPIN** (D), **HOLD** (D#, while held), **DROP 1-4** (E, F, G, A).
  - **The pitch wheel is your hand on the record**: push it and the record moves forwards, pull it and it goes back,
    as far as **REACH**; let it spring back and the record comes back with it (a baby scratch). Left at its centre a
    moment, the hand lets go and the motor takes the record on again.
  - Or grab the **platter** on the page with the mouse and drag round it.
- **The motor**: **START** (how long it takes to come up to speed) and **BRAKE** (how long to stop; a long BRAKE is a
  tape-stop wind-down), the **PITCH** fader (+-16%) and **33 / 45 RPM**. A slipmat under the record: let go of it and
  it catches up with the turning platter in about 20 ms.
- **BACKSPIN**: the record thrown back at **SPIN** times its speed, slowing to a stop over **SPIN TIME**.
- **HOLD**: a hand holding the record still; let go and it's straight back up to speed. With DROP: stabs.
- **Needle drops** at **CUE 1-4** (anywhere on the record), with the needle's thump and crackle (**NEEDLE**; louder for
  a harder key). The jump fades the old place out under the new in 4 ms, so it never clicks.
- **It sounds like a turntable**:
  - The record's loudness follows its speed, as a real cartridge's does: a slow drag is quiet, a fast scratch loud
    (up to +6 dB), a record held still silent. At normal speed it's exactly the sample's level.
  - It's clean at any speed: a sped-up sample's highs would fold back down as harsh, grainy noise (aliasing). The
    deck keeps the record filtered to a half, a quarter and an eighth of its band and reads it through a high-quality
    interpolator whose cutoff follows the speed. Measured: a 15 kHz tone read twice as fast is more than 60 dB down;
    tones that fit pass at full level.
- **Records**: **LOAD RECORD...** puts on a sample of yours (WAV, AIFF, FLAC or Ogg, up to 3 minutes), remembered
  with the project (if the file goes missing the page says so and plays the house record meanwhile). **THE HOUSE
  RECORD** is ours, built in: an "ahh", a horn stab, a short drum break and a 1 kHz tone, two seconds apart, one at
  each cue's starting place. All synthesized by PR7-T: no one's recording.
- **The page**: the platter (turning as the record does, the arm moving in as it plays through), the record as a
  strip with the needle and the cues, the keys as buttons, the speed (and BACKWARDS), and the knobs.
- The deck goes through everything after the synth: the rack (FX), OUTPUT and the LIMITER.
- Not yet: the crossfader (stage 3), the scratch library (stage 4), a bigger platter view (stage 5), the DJ mixer's
  sound (stage 6). The deck doesn't feed the sends or the insert buses yet, and presets don't store the record (the
  project does).

## 0.1.0

**Paragon PR7-T: the turntable instrument of the PR7 family, starting from PR7-K 0.74.1.**

- Its own plugin: "Paragon PR7-T" (plugin code Pr7t, VST3 category Instrument Sampler), so it installs beside PR7-K
  and its other siblings. Its own presets folder (`Paragon PR7-T`) and settings file: nothing is shared with PR7-K's.
- A "-T" logo built from the P's parts, in the family's slanted, speed-cut lettering.
- Everything else is PR7-K 0.74.1's: the synthesizer, the effects rack, the presets, MIMIC, the arpeggiators.
