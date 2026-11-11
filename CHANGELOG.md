# Changelog

Each version's notes appear on its GitHub Release page.

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
