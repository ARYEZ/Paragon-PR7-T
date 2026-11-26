#include "ScratchDemos.h"

#include "dsp/deck/Deck.h"

#include <algorithm>
#include <cmath>

namespace scratchdemos
{
    namespace
    {
        // The deck's keys (deck A, from C1 = note 24) and the scratch library's (from C2 = 36), as the presets set them.
        enum : int { play = 24, stop = 25, backspin = 26, hold = 27, drop1 = 28, drop2 = 29, cut = 30, drop3 = 31, transform = 32, drop4 = 33 };
        enum : int { baby = 36, forward, backward, chirp, stab, drag, tear, scribble, transformer, flare, flare2, crab };

        void press (std::vector<Event>& e, double from, double to, int note, float velocity = 1.0f)
        {
            e.push_back ({ from, juce::MidiMessage::noteOn (1, note, velocity) });
            e.push_back ({ to, juce::MidiMessage::noteOff (1, note) });
        }
    }

    std::vector<Event> forPreset (const juce::String& name, double rate)
    {
        std::vector<Event> e;
        // (0.12.2) A scratch key held for several of its moves is written as one note a move, each starting where the
        // move does: the file shows every flare, chirp or crab where it lands (a held key plays them back to back, so
        // the sound is the same; 0.12.1 wrote one long note, which looked like nothing was there). Each note lets go
        // just before its move ends, so the move finishes and the next note starts the next one.
        const auto move = [&e, rate] (double from, double to, int key, float velocity = 1.0f)
        {
            const auto cycle = deck::scratchBeats (key - baby) / juce::jmax (0.25, rate);
            if (cycle >= to - from - 0.06)
            {
                press (e, from, to, key, velocity);
                return;
            }
            for (auto start = from; start < to - 0.02; start += cycle)
                press (e, start, juce::jmin (to, start + 0.95 * cycle), key, velocity);
        };
        // Two bars, the second answering the first: the needle dropped on a beat, a move held through two beats, a
        // last one on beat four.
        const auto twoBars = [&e, &move] (int dropKey, int first, int second)
        {
            for (int bar = 0; bar < 2; ++bar)
            {
                const auto at = 4.0 * bar;
                press (e, at, at + 0.1, dropKey);
                move (at + 1.0, at + 2.95, bar == 0 ? first : second);
                move (at + 3.0, at + 3.95, first);
            }
        };
        if (name == "Chirp Stabs")            twoBars (drop1, chirp, stab);
        else if (name == "Dragged Wail")      twoBars (drop1, baby, drag);
        else if (name == "Flare Cutter")      twoBars (drop2, flare2, flare);
        else if (name == "Crab Burst")        twoBars (drop2, crab, crab);
        else if (name == "Scribble Buzz")     twoBars (drop3, scribble, scribble);
        else if (name == "Tear Drag")         twoBars (drop2, tear, drag);
        else if (name == "Radio Scratch")     twoBars (drop1, baby, chirp);
        else if (name == "Transformer Riff")
        {
            // (the chugs last two and a half beats)
            press (e, 0.0, 0.1, drop4);
            press (e, 0.5, 2.4, transform);
            press (e, 4.0, 4.1, drop4);
            move (4.5, 6.4, transformer);
        }
        else if (name == "Feedback Rider")
        {
            press (e, 0.0, 0.1, drop3);
            press (e, 1.0, 1.1, stop);
            // The mod wheel worked up and down over a bar (MOD HAND moves the record), then a DRAG.
            for (int k = 0; k <= 32; ++k)
                e.push_back ({ 1.5 + k * 0.1, juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (63.5 + 60.0 * std::sin (k * 0.6))) });
            press (e, 5.0, 5.1, drop3);
            move (5.25, 7.5, drag);
        }
        else if (name == "Spinback Ender")
        {
            press (e, 0.0, 0.1, drop4);
            press (e, 2.0, 2.1, backspin);
            press (e, 4.0, 4.1, drop2);
            press (e, 4.75, 4.85, stop);
        }
        else if (name == "Needle Drop Hits")
        {
            const int hits[] = { drop2, drop2, drop4, drop1, drop2, drop3, drop4, drop2 };
            for (int k = 0; k < 8; ++k)
                press (e, k, k + 0.1, hits[k]);
            press (e, 7.5, 7.9, cut);
        }
        else if (name == "Live Synth Cuts")
        {
            // A stab on the synth (E3 A3 B3), then scratched off THE LIVE RECORD; again on G3.
            for (int bar = 0; bar < 2; ++bar)
            {
                const auto at = 4.0 * bar + 0.5;
                for (const auto n : bar == 0 ? std::vector<int> { 52, 57, 59 } : std::vector<int> { 55, 59, 62 })
                    press (e, at, at + 0.5, n, 0.9f);
                move (at + 1.25, at + 2.9, bar == 0 ? chirp : flare2);
                move (at + 3.0, at + 3.9, crab);
            }
        }
        // On the file's grid (960 ticks a beat), so the file and the clip put every event on the same sample.
        for (auto& event : e)
            event.beat = std::round (event.beat * ticksPerBeat) / ticksPerBeat;
        std::stable_sort (e.begin(), e.end(), [] (const Event& a, const Event& b) { return a.beat < b.beat; });
        return e;
    }

    juce::MidiFile toMidiFile (const juce::String& name, const std::vector<Event>& events, double bpm)
    {
        juce::MidiMessageSequence track;
        auto title = juce::MidiMessage::textMetaEvent (3, name);
        title.setTimeStamp (0.0);
        track.addEvent (title);
        // (0.12.2) What it needs, where a DAW shows it: these keys scratch only on the deck this preset turns on.
        auto marker = juce::MidiMessage::textMetaEvent (6, "Play on PR7-T with the preset " + name + " loaded");
        marker.setTimeStamp (0.0);
        track.addEvent (marker);
        auto tempo = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (1.0, bpm)));
        tempo.setTimeStamp (0.0);
        track.addEvent (tempo);
        auto metre = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
        metre.setTimeStamp (0.0);
        track.addEvent (metre);
        double last = 0.0;
        for (const auto& event : events)
        {
            auto message = event.message;
            message.setTimeStamp (std::round (event.beat * ticksPerBeat));
            track.addEvent (message);
            last = juce::jmax (last, message.getTimeStamp());
        }
        track.updateMatchedPairs();
        auto end = juce::MidiMessage::endOfTrack();
        end.setTimeStamp (last);
        track.addEvent (end);
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ticksPerBeat);
        file.addTrack (track);
        return file;
    }
}
