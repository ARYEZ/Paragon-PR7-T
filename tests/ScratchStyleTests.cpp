// NU SCHOOL (PR7-T 0.12.0): THE DECK's scratch-style presets, each played the way its notes say. Checked: each one
// sounds, stays clean (no NaN, no clipping) and goes quiet once it's let be. `[.scratchrender]` writes a clip of each
// (PARAGON_RENDER names the folder) for listening.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"
#include "rigs/Expansions.h"
#include "rigs/FactoryRigs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr double bpm = 95.0;
    double beats (double b) { return b * 60.0 / bpm; }

    // The deck's keys (deck A, from C1) and the scratch library's (from C2).
    enum : int { play = 24, stop = 25, backspin = 26, hold = 27, drop1 = 28, drop2 = 29, cut = 30, drop3 = 31, transform = 32, drop4 = 33 };
    enum : int { baby = 36, forward, backward, chirp, stab, drag, tear, scribble, transformer, flare, flare2, crab };

    using Events = std::vector<std::pair<double, juce::MidiMessage>>;
    void press (Events& e, double fromBeat, double toBeat, int note, float velocity = 1.0f)
    {
        e.push_back ({ beats (fromBeat), juce::MidiMessage::noteOn (1, note, velocity) });
        e.push_back ({ beats (toBeat), juce::MidiMessage::noteOff (1, note) });
    }

    // Each preset's routine, as its notes describe it: two bars, the second answering the first.
    Events routineFor (const juce::String& name)
    {
        Events e;
        const auto twoBars = [&e] (int dropKey, int first, int second)
        {
            for (int bar = 0; bar < 2; ++bar)
            {
                const auto at = 4.0 * bar;
                press (e, at, at + 0.1, dropKey);
                press (e, at + 1.0, at + 2.95, bar == 0 ? first : second);
                press (e, at + 3.0, at + 3.95, first);
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
            press (e, 4.5, 6.4, transformer);
        }
        else if (name == "Feedback Rider")
        {
            press (e, 0.0, 0.1, drop3);
            press (e, 1.0, 1.1, stop);
            // The mod wheel worked up and down over a bar, then a DRAG.
            for (int k = 0; k <= 32; ++k)
                e.push_back ({ beats (1.5 + k * 0.1), juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (63.5 + 60.0 * std::sin (k * 0.6))) });
            press (e, 5.0, 5.1, drop3);
            press (e, 5.25, 7.5, drag);
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
            // A stab on the synth (E3 A3 B3), then scratched; again on G3.
            for (int bar = 0; bar < 2; ++bar)
            {
                const auto at = 4.0 * bar + 0.5;
                for (const auto n : bar == 0 ? std::vector<int> { 52, 57, 59 } : std::vector<int> { 55, 59, 62 })
                    press (e, at, at + 0.5, n, 0.9f);
                press (e, at + 1.25, at + 2.9, bar == 0 ? chirp : flare2);
                press (e, at + 3.0, at + 3.9, crab);
            }
        }
        std::stable_sort (e.begin(), e.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        return e;
    }

    std::vector<Rig> nuSchool()
    {
        std::vector<Rig> rigs;
        for (const auto& rig : makeExpansionRigs())
            if (rig.tags.contains ("NU SCHOOL"))
                rigs.push_back (rig);
        return rigs;
    }

    // Plays `rig` through its routine (two bars, then two seconds to ring out), the song stopped.
    void perform (const Rig& rig, std::vector<float>& left, std::vector<float>& right)
    {
        ParagonX1AudioProcessor p;
        p.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
        p.seedRandomForTest (1);
        p.loadRig (RigLibrary::Entry { rig, {}, false });
        const auto events = routineFor (rig.name);
        const auto total = (int) ((beats (8.0) + 2.0) * sampleRate);
        juce::AudioBuffer<float> buffer (2, blockSize);
        size_t next = 0;
        for (int at = 0; at < total; at += blockSize)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            for (; next < events.size() && events[next].first * sampleRate < at + blockSize; ++next)
                midi.addEvent (events[next].second, juce::jlimit (0, blockSize - 1, (int) (events[next].first * sampleRate) - at));
            p.processBlock (buffer, midi);
            left.insert (left.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            right.insert (right.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + blockSize);
        }
    }
}

TEST_CASE ("NU SCHOOL: each scratch style plays its routine: it sounds, stays clean and goes quiet", "[nuschool][expansions]")
{
    const auto rigs = nuSchool();
    REQUIRE (rigs.size() == 12);
    for (const auto& rig : rigs)
    {
        INFO (rig.name);
        REQUIRE_FALSE (routineFor (rig.name).empty());
        std::vector<float> left, right;
        perform (rig, left, right);
        float loudest = 0.0f;
        bool finite = true;
        for (size_t i = 0; i < left.size(); ++i)
        {
            finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
            loudest = juce::jmax (loudest, std::abs (left[i]), std::abs (right[i]));
        }
        const auto scratched = rms (left, (size_t) (beats (1.0) * sampleRate), (size_t) (beats (8.0) * sampleRate));
        INFO ("peak " << juce::Decibels::gainToDecibels (loudest) << " dBFS, the routine's rms " << juce::Decibels::gainToDecibels ((float) scratched) << " dB");
        CHECK (finite);
        CHECK (juce::Decibels::gainToDecibels (loudest) > -24.0f);
        CHECK (juce::Decibels::gainToDecibels (loudest) < 0.0f);
        CHECK (juce::Decibels::gainToDecibels ((float) scratched) > -40.0f);
    }
}

TEST_CASE ("NU SCHOOL: a clip of each", "[.scratchrender]")
{
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", "."));
    folder.createDirectory();
    for (const auto& rig : nuSchool())
    {
        std::vector<float> left, right;
        perform (rig, left, right);
        juce::AudioBuffer<float> out (2, (int) left.size());
        out.copyFrom (0, 0, left.data(), (int) left.size());
        out.copyFrom (1, 0, right.data(), (int) right.size());
        const auto file = folder.getChildFile (rig.getSlotName() + " " + rig.name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), sampleRate, 2, 24, {}, 0));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        std::printf ("%s: peak %.1f dBFS\n", rig.name.toRawUTF8(), juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples())));
    }
}
