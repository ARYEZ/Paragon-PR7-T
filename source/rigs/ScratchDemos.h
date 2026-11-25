#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

// NU SCHOOL's demos (PR7-T 0.12.1, the owner's ask: "add the midi file drag out option"): each scratch-style preset's
// routine, the notes its demo clip played, to drag onto a track and see how it's played. In beats: the deck's moves
// follow the song's tempo, so the routine plays the same at any tempo (the clips sent with 0.12.0 put these notes at
// 95 BPM while the moves ran at 120: an unplayed render's tempo; at 95 BPM a move is a little slower than in them).
namespace scratchdemos
{
    struct Event
    {
        double beat = 0.0;
        juce::MidiMessage message;
    };

    // The routine of the NU SCHOOL preset named `presetName` (none: empty), sorted by beat.
    std::vector<Event> forPreset (const juce::String& presetName);

    // As a type 1 MIDI file: one track named after the preset, 960 ticks a beat, `bpm` as its tempo, 4/4.
    juce::MidiFile toMidiFile (const juce::String& presetName, const std::vector<Event>&, double bpm = 95.0);

    inline constexpr int ticksPerBeat = 960;
}
