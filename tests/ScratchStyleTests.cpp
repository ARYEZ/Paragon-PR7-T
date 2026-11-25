// NU SCHOOL (PR7-T 0.12.0): THE DECK's scratch-style presets, each played the way its notes say. Checked: each one
// sounds, stays clean (no NaN, no clipping) and goes quiet once it's let be. `[.scratchrender]` writes a clip of each
// (PARAGON_RENDER names the folder) for listening.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"
#include "rigs/Expansions.h"
#include "rigs/FactoryRigs.h"
#include "rigs/ScratchDemos.h"
#include "ui/DeckPage.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    constexpr double bpm = 95.0;
    double beats (double b) { return b * 60.0 / bpm; }

    using Events = std::vector<std::pair<double, juce::MidiMessage>>;

    // Each preset's routine (0.12.1: rigs/ScratchDemos, the same notes DEMO MIDI gives), in seconds at the clips' 95 BPM.
    Events routineFor (const juce::String& name)
    {
        Events e;
        for (const auto& event : scratchdemos::forPreset (name))
            e.push_back ({ beats (event.beat), event.message });
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

TEST_CASE ("NU SCHOOL: DEMO MIDI gives the routine its demo played, on the deck page while a NU SCHOOL preset is loaded", "[nuschool][expansions]")
{
    ParagonX1AudioProcessor p;
    p.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
    DeckPage page (p);
    page.setSize (1476, 672);
    page.refreshForTest();
    CHECK (p.getDemoMidiPreset().isEmpty());          // Init: no demo
    CHECK_FALSE (page.getDemoButton().isVisible());

    const auto rigs = nuSchool();
    const auto flare = std::find_if (rigs.begin(), rigs.end(), [] (const Rig& r) { return r.name == "Flare Cutter"; });
    REQUIRE (flare != rigs.end());
    p.loadRig (RigLibrary::Entry { *flare, {}, true });
    page.refreshForTest();
    CHECK (p.getDemoMidiPreset() == "Flare Cutter");
    CHECK (page.getDemoButton().isVisible());
    CHECK (page.getDemoButton().fileName == "Flare Cutter (demo)");

    // The file: its notes where the routine has them, in beats (960 ticks), the deck's keys and the scratch keys.
    const auto file = p.demoMidiDragFile();
    REQUIRE (file.existsAsFile());
    juce::FileInputStream in (file);
    juce::MidiFile midi;
    REQUIRE (midi.readFrom (in));
    CHECK (midi.getTimeFormat() == scratchdemos::ticksPerBeat);
    REQUIRE (midi.getNumTracks() == 1);
    std::vector<std::pair<double, int>> ons;
    const auto* track = midi.getTrack (0);
    for (int i = 0; i < track->getNumEvents(); ++i)
        if (const auto& m = track->getEventPointer (i)->message; m.isNoteOn())
            ons.push_back ({ m.getTimeStamp() / scratchdemos::ticksPerBeat, m.getNoteNumber() });
    const std::vector<std::pair<double, int>> expected { { 0.0, 29 }, { 1.0, 46 }, { 3.0, 46 }, { 4.0, 29 }, { 5.0, 45 }, { 7.0, 46 } };
    REQUIRE (ons.size() == expected.size());
    for (size_t k = 0; k < ons.size(); ++k)
    {
        CHECK_THAT (ons[k].first, Catch::Matchers::WithinAbs (expected[k].first, 1.0e-9));
        CHECK (ons[k].second == expected[k].second);
    }
    // Every note ends (a held move let go).
    int offs = 0;
    for (int i = 0; i < track->getNumEvents(); ++i)
        offs += track->getEventPointer (i)->message.isNoteOff() ? 1 : 0;
    CHECK (offs == (int) expected.size());

    // Every NU SCHOOL preset has one; no other preset does.
    for (const auto& rig : rigs)
        CHECK_FALSE (scratchdemos::forPreset (rig.name).empty());
    CHECK (scratchdemos::forPreset ("Supersaw Lead").empty());
}
