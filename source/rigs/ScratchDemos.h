#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

// NU SCHOOL's demos (PR7-T 0.12.1, the owner's ask: "add the midi file drag out option"): each scratch-style preset's
// routine, the notes its demo clip played, to drag onto a track and see how it's played. In beats: the deck's moves
// follow the song's tempo, so the routine plays the same at any tempo. (0.12.2) The clips are rendered as a DAW plays
// this file at 95 BPM, so the file reproduces its clip sample for sample (tested); the clips sent with 0.12.0 put the
// notes at 95 BPM while the moves ran at 120, an unplayed render's tempo, so no file could match them.
namespace scratchdemos
{
    struct Event
    {
        double beat = 0.0;
        juce::MidiMessage message;
    };

    // The routine of the NU SCHOOL preset named `presetName` (none: empty), sorted by beat. `scratchRate` is the deck's
    // RATE (x0.5, x1, x2): a held move is written one note a move, and RATE sets how long a move is.
    std::vector<Event> forPreset (const juce::String& presetName, double scratchRate = 1.0);

    // As a type 1 MIDI file: one track named after the preset, 960 ticks a beat, `bpm` as its tempo, 4/4.
    juce::MidiFile toMidiFile (const juce::String& presetName, const std::vector<Event>&, double bpm = 95.0);

    inline constexpr int ticksPerBeat = 960;
}
