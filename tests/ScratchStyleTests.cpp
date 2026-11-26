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

    using Events = std::vector<std::pair<double, juce::MidiMessage>>;   // (beat, message)

    // The deck's RATE as the preset sets it (x0.5, x1, x2).
    double rateOf (const Rig& rig)
    {
        const auto found = rig.parameters.find (ParamIDs::deckScratchRate);
        const auto index = found != rig.parameters.end() ? juce::roundToInt (found->second) : 1;
        return deck::scratchRates[juce::jlimit (0, (int) std::size (deck::scratchRates) - 1, index)];
    }

    // Each preset's routine (0.12.1: rigs/ScratchDemos, the same notes DEMO MIDI gives), in beats.
    Events routineFor (const Rig& rig)
    {
        Events e;
        for (const auto& event : scratchdemos::forPreset (rig.name, rateOf (rig)))
            e.push_back ({ event.beat, event.message });
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

    // A DAW's transport: the song playing at `bpm`, moved on block by block.
    struct DawSong final : juce::AudioPlayHead
    {
        double bpm = 95.0, ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (true);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return info;
        }
    };

    // Plays `events` on `rig` (none: the plugin as it opens) as a DAW plays a MIDI track: the song running at `tempo`
    // from beat 0, each event on the sample its beat falls on; eight beats, then two seconds to ring out. (0.12.2:
    // the clips are made so, so the DEMO MIDI file reproduces them; before, the clips had no song and the moves ran
    // at 120 BPM under notes placed at 95.)
    juce::AudioBuffer<float> playEvents (const Events& events, const Rig* rig, double tempo = bpm)
    {
        ParagonX1AudioProcessor p;
        DawSong song;
        song.bpm = tempo;
        p.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
        p.setPlayHead (&song);
        p.seedRandomForTest (1);
        if (rig != nullptr)
            p.loadRig (RigLibrary::Entry { *rig, {}, true });
        const auto samplesPerBeat = 60.0 / tempo * sampleRate;
        const auto total = (int) (8.0 * samplesPerBeat + 2.0 * sampleRate) / blockSize * blockSize;
        juce::AudioBuffer<float> out (2, total), buffer (2, blockSize);
        size_t next = 0;
        for (int at = 0; at < total; at += blockSize)
        {
            song.ppq = at / samplesPerBeat;
            buffer.clear();
            juce::MidiBuffer midi;
            for (; next < events.size() && events[next].first * samplesPerBeat < at + blockSize; ++next)
                midi.addEvent (events[next].second, juce::jlimit (0, blockSize - 1, juce::roundToInt (events[next].first * samplesPerBeat) - at));
            p.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, at, buffer, c, 0, blockSize);
        }
        return out;
    }

    // Plays `rig` through its routine, as its demo clip.
    juce::AudioBuffer<float> perform (const Rig& rig) { return playEvents (routineFor (rig), &rig); }

    // A MIDI file's notes and controllers, by beat.
    Events eventsIn (const juce::File& file)
    {
        juce::FileInputStream in (file);
        juce::MidiFile midi;
        midi.readFrom (in);
        Events events;
        const auto* track = midi.getTrack (0);
        for (int i = 0; i < track->getNumEvents(); ++i)
            if (const auto& m = track->getEventPointer (i)->message; m.isNoteOnOrOff() || m.isController())
                events.push_back ({ m.getTimeStamp() / midi.getTimeFormat(), m });
        return events;
    }

    // The DEMO MIDI file the deck page's button gives for `rig`, written while a DAW plays at `tempo`.
    juce::File demoFileFor (const Rig& rig, double tempo = bpm)
    {
        ParagonX1AudioProcessor p;
        DawSong song;
        song.bpm = tempo;
        p.setPlayConfigDetails (0, 2, sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
        p.setPlayHead (&song);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer none;
        p.processBlock (buffer, none);   // (reads the DAW's tempo)
        p.loadRig (RigLibrary::Entry { rig, {}, true });
        return p.demoMidiDragFile();
    }

    // How many times the sound is cut and comes back (5 ms windows; under a tenth of the loudest is cut), in [from, to).
    int cutsIn (const juce::AudioBuffer<float>& b, int from, int to)
    {
        const int w = (int) (0.005 * sampleRate);
        float loudest = 0.0f;
        for (int i = from; i + w <= to; i += w)
            loudest = juce::jmax (loudest, b.getRMSLevel (0, i, w));
        int cuts = 0;
        bool open = true;
        for (int i = from; i + w <= to; i += w)
        {
            const auto level = b.getRMSLevel (0, i, w);
            if (open && level < loudest * 0.1f) { open = false; ++cuts; }
            else if (! open && level > loudest * 0.3f) open = true;
        }
        return cuts;
    }

    void writeWav (const juce::AudioBuffer<float>& b, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), sampleRate, 2, 24, {}, 0));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}

TEST_CASE ("NU SCHOOL: each scratch style plays its routine: it sounds, stays clean and goes quiet", "[nuschool][expansions]")
{
    const auto rigs = nuSchool();
    REQUIRE (rigs.size() == 12);
    for (const auto& rig : rigs)
    {
        INFO (rig.name);
        REQUIRE_FALSE (routineFor (rig).empty());
        const auto out = perform (rig);
        bool finite = true;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < out.getNumSamples(); ++i)
                finite = finite && std::isfinite (out.getSample (c, i));
        const auto loudest = out.getMagnitude (0, out.getNumSamples());
        const auto scratched = out.getRMSLevel (0, (int) (beats (1.0) * sampleRate), (int) (beats (7.0) * sampleRate));
        INFO ("peak " << juce::Decibels::gainToDecibels (loudest) << " dBFS, the routine's rms " << juce::Decibels::gainToDecibels (scratched) << " dB");
        CHECK (finite);
        CHECK (juce::Decibels::gainToDecibels (loudest) > -24.0f);
        CHECK (juce::Decibels::gainToDecibels (loudest) < 0.0f);
        CHECK (juce::Decibels::gainToDecibels (scratched) > -40.0f);
    }
}

TEST_CASE ("NU SCHOOL: each preset's DEMO MIDI file, played in a DAW with the preset loaded, is its demo clip sample for sample", "[nuschool][expansions]")
{
    // (0.12.2, the owner: "I wanted the MIDI to reproduce the Flare Cutter demo scratching perfectly".) The file the
    // deck page's button writes, read back and played as a DAW plays a track at 95 BPM, against the clip.
    for (const auto& rig : nuSchool())
    {
        INFO (rig.name);
        const auto clip = perform (rig);
        const auto fromFile = playEvents (eventsIn (demoFileFor (rig)), &rig);
        REQUIRE (fromFile.getNumSamples() == clip.getNumSamples());
        float worst = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < clip.getNumSamples(); ++i)
                worst = juce::jmax (worst, std::abs (clip.getSample (c, i) - fromFile.getSample (c, i)));
        CHECK (worst == 0.0f);
    }
}

TEST_CASE ("NU SCHOOL: Flare Cutter's DEMO MIDI scratches only with the preset (its deck) loaded; on the plugin as it opens it plays synth notes", "[nuschool][expansions]")
{
    const auto rigs = nuSchool();
    const auto flare = std::find_if (rigs.begin(), rigs.end(), [] (const Rig& r) { return r.name == "Flare Cutter"; });
    REQUIRE (flare != rigs.end());
    const auto events = eventsIn (demoFileFor (*flare));
    const auto beat = juce::roundToInt (60.0 / bpm * sampleRate);
    const auto scratched = playEvents (events, &*flare);
    const auto plain = playEvents (events, nullptr);
    // Bar 1, beats 2-4: two 2-CLICK FLAREs then a FLARE, each clicking the fader shut several times.
    const auto cuts = cutsIn (scratched, beat, 4 * beat);
    INFO ("cuts with the preset " << cuts << ", without " << cutsIn (plain, beat, 4 * beat));
    CHECK (cuts >= 10);
    CHECK (cutsIn (plain, beat, 4 * beat) <= 1);
}

TEST_CASE ("NU SCHOOL: a clip of each, and its DEMO MIDI file", "[.scratchrender]")
{
    const juce::File folder (juce::SystemStats::getEnvironmentVariable ("PARAGON_RENDER", "."));
    folder.createDirectory();
    for (const auto& rig : nuSchool())
    {
        const auto out = perform (rig);
        writeWav (out, folder.getChildFile (rig.getSlotName() + " " + rig.name + ".wav"));
        demoFileFor (rig).copyFileTo (folder.getChildFile (rig.getSlotName() + " " + rig.name + " (demo).mid"));
        std::printf ("%s: peak %.1f dBFS, rms %.1f dB\n", rig.name.toRawUTF8(), juce::Decibels::gainToDecibels (out.getMagnitude (0, out.getNumSamples())),
                     juce::Decibels::gainToDecibels (out.getRMSLevel (0, (int) (beats (1.0) * sampleRate), (int) (beats (7.0) * sampleRate))));
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
    // (0.12.2) one note a move: the drop on beat 1, a 2-CLICK FLARE (a beat long) on beats 2 and 3, a FLARE (A#2's
    // answer in bar 2) likewise, the last move on beat 4.
    const std::vector<std::pair<double, int>> expected { { 0.0, 29 }, { 1.0, 46 }, { 2.0, 46 }, { 3.0, 46 },
                                                         { 4.0, 29 }, { 5.0, 45 }, { 6.0, 45 }, { 7.0, 46 } };
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
        CHECK_FALSE (scratchdemos::forPreset (rig.name, rateOf (rig)).empty());
    CHECK (scratchdemos::forPreset ("Supersaw Lead").empty());
}
