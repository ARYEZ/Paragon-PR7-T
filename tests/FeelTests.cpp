// FEEL and DIRT (PR7-T 0.12.0): PR7-K's SCRATCH brought to the deck, the owner's choice after an A/B. What's checked:
// off, the deck is exactly as before (FEEL's fade at 0 touches nothing); on, a record at normal speed comes back about
// as loud, driven (odd harmonics, more with DIRT), a cut is still silence, a slowed record fades softer than the plain
// cartridge, each technique starts a little late and strokes a little differently; new decks have it on, projects and
// presets from before 0.12.0 load with it off.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"
#include "rigs/Rig.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    std::shared_ptr<const deck::Record> sineRecord (double hz, double seconds = 10.0, float level = 0.3f)
    {
        juce::AudioBuffer<float> audio (1, (int) (seconds * sampleRate));
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample (0, i, level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate));
        return deck::makeRecord (audio, sampleRate, "sine");
    }

    struct Turntable
    {
        deck::Deck deck;
        deck::Settings settings;
        std::shared_ptr<const deck::Record> record;
        juce::MidiBuffer pending;
        std::vector<float> out;

        explicit Turntable (std::shared_ptr<const deck::Record> r, bool feel, float dirt = 0.0f) : record (std::move (r))
        {
            deck.prepare (sampleRate);
            deck.seedRandom (1);
            settings.needle = 0.0f;
            settings.startSeconds = 0.05f;
            settings.feel = feel;
            settings.dirt = dirt;
        }
        void key (int note, bool down = true, float velocity = 1.0f)
        {
            pending.addEvent (down ? juce::MidiMessage::noteOn (1, note, velocity) : juce::MidiMessage::noteOff (1, note), 0);
        }
        void key (deck::Key k, bool down = true) { key (settings.firstKey + (int) k, down); }
        size_t play (double seconds)
        {
            const auto from = out.size();
            std::vector<float> left ((size_t) blockSize), right ((size_t) blockSize);
            for (int b = 0; b < (int) std::round (seconds * sampleRate / blockSize); ++b)
            {
                std::fill (left.begin(), left.end(), 0.0f);
                std::fill (right.begin(), right.end(), 0.0f);
                deck.process (left.data(), right.data(), blockSize, pending, 0, record.get(), settings);
                pending.clear();
                out.insert (out.end(), left.begin(), left.end());
            }
            return from;
        }
    };

    // The level of harmonic `h` of `hz` in out[from, to), by a Goertzel over a Hann window.
    double harmonicLevel (const std::vector<float>& x, size_t from, size_t to, double hz)
    {
        const auto n = (double) (to - from);
        double re = 0.0, im = 0.0;
        for (size_t i = from; i < to; ++i)
        {
            const auto w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - from) / n);
            const auto phase = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
            re += w * x[i] * std::cos (phase);
            im += w * x[i] * std::sin (phase);
        }
        return std::sqrt (re * re + im * im) / n;
    }
}

TEST_CASE ("FEEL off: the deck exactly as before, sample for sample", "[feel]")
{
    // The engine's default is off: a deck with FEEL off and DIRT moved is the same as one with neither set.
    Turntable plain (sineRecord (440.0), false), dirty (sineRecord (440.0), false, 0.8f);
    for (auto* t : { &plain, &dirty })
    {
        t->key (deck::Key::play);
        t->play (0.5);
        t->key (36 + 3);   // a CHIRP
        t->play (0.6);
    }
    REQUIRE (plain.out.size() == dirty.out.size());
    for (size_t i = 0; i < plain.out.size(); ++i)
        if (plain.out[i] != dirty.out[i])
        {
            FAIL ("sample " << i << " differs");
            break;
        }
}

TEST_CASE ("FEEL on: a record at normal speed comes back about as loud, driven; DIRT drives it harder; a cut is silence", "[feel]")
{
    const auto measure = [] (bool feel, float dirt)
    {
        Turntable t (sineRecord (220.0, 10.0, 0.28f), feel, dirt);
        t.key (deck::Key::play);
        t.play (1.0);
        const auto from = (size_t) (0.5 * sampleRate), to = (size_t) (1.0 * sampleRate);
        return std::array<double, 3> { rms (t.out, from, to), harmonicLevel (t.out, from, to, 660.0) / harmonicLevel (t.out, from, to, 220.0),
                                       harmonicLevel (t.out, from, to, 1100.0) / harmonicLevel (t.out, from, to, 220.0) };
    };
    const auto clean = measure (false, 0.0f), feel = measure (true, 0.0f), dirt = measure (true, 1.0f);
    INFO ("rms " << clean[0] << " / " << feel[0] << " / " << dirt[0] << ", 3rd " << clean[1] << " / " << feel[1] << " / " << dirt[1]
          << ", 5th " << clean[2] << " / " << feel[2] << " / " << dirt[2]);
    // About as loud (within 3 dB of the record).
    CHECK (std::abs (juce::Decibels::gainToDecibels (feel[0] / clean[0])) < 3.0);
    CHECK (std::abs (juce::Decibels::gainToDecibels (dirt[0] / clean[0])) < 3.0);
    // Clean: no 3rd harmonic. FEEL: a strong one (the drive squares it off); DIRT: more of the 5th.
    CHECK (clean[1] < 0.001);
    CHECK (feel[1] > 0.1);
    CHECK (dirt[2] > feel[2] * 1.5);

    // LEVEL stays the channel's level with FEEL on (taken out before the drive, put back after): +6 dB is +6 dB.
    {
        Turntable quiet (sineRecord (220.0, 10.0, 0.28f), true), loud (sineRecord (220.0, 10.0, 0.28f), true);
        loud.settings.levelDb = 6.0f;
        for (auto* t : { &quiet, &loud })
        {
            t->key (deck::Key::play);
            t->play (1.0);
        }
        const auto from = (size_t) (0.5 * sampleRate);
        CHECK_THAT (juce::Decibels::gainToDecibels (rms (loud.out, from, loud.out.size()) / rms (quiet.out, from, quiet.out.size())),
                    Catch::Matchers::WithinAbs (6.0, 0.3));
    }

    // The crossfader comes after the drive and the hiss: cut, silence.
    Turntable cut (sineRecord (220.0), true);
    cut.settings.fader = 0.0f;
    cut.key (deck::Key::play);
    cut.play (0.5);
    CHECK (rms (cut.out, (size_t) (0.1 * sampleRate), cut.out.size()) == 0.0);
}

TEST_CASE ("FEEL: a slowed record fades softly (the cartridge), never louder than the record", "[feel]")
{
    // The record at a quarter speed (the brake caught part way): the plain cartridge gives a quarter of the level,
    // FEEL's about (0.25 / 0.6)^0.6 = 0.59 of it. On a quiet record (the drive near linear), the mixer's EQ taken out:
    // slowed, the 1 kHz tone is heard at 250 Hz, so FEEL's level of a 250 Hz record against a 1 kHz one at normal
    // speed corrects for it.
    const auto heldLevel = [] (double hz)
    {
        Turntable t (sineRecord (hz, 10.0, 0.01f), true);
        t.key (deck::Key::play);
        t.play (0.5);
        return rms (t.out, t.out.size() - (size_t) (0.1 * sampleRate), t.out.size());
    };
    const auto eq = heldLevel (250.0) / heldLevel (1000.0);
    for (const auto feel : { false, true })
    {
        Turntable t (sineRecord (1000.0, 10.0, 0.01f), feel);
        t.settings.brakeSeconds = 4.0f;
        t.key (deck::Key::play);
        t.play (0.5);
        const auto full = rms (t.out, t.out.size() - (size_t) (0.1 * sampleRate), t.out.size());
        t.key (deck::Key::stop);
        t.play (3.0);   // 3 s of a 4 s brake: a quarter of the speed left
        const auto slow = rms (t.out, t.out.size() - (size_t) (0.05 * sampleRate), t.out.size());
        const auto ratio = slow / full;
        INFO ((feel ? "FEEL " : "plain ") << ratio << ", EQ " << eq);
        if (feel)
            CHECK_THAT (ratio / eq, Catch::Matchers::WithinAbs (std::pow (0.25 / 0.6, 0.6), 0.06));
        else
            CHECK_THAT (ratio, Catch::Matchers::WithinAbs (0.25, 0.03));
    }
}

TEST_CASE ("FEEL: each technique a hand's: a few ms late, its stroke a little longer or shorter each time", "[feel]")
{
    // A FORWARD (the push only, heard) played four times: without FEEL each the same; with FEEL each starts 0-5 ms late
    // and reaches a different distance (the record's place at the stroke's far end).
    for (const auto feel : { false, true })
    {
        Turntable t (sineRecord (440.0), feel);
        std::vector<double> starts, reaches;
        for (int k = 0; k < 4; ++k)
        {
            t.play (0.2);
            const auto before = t.deck.getPositionSeconds();
            const auto from = t.out.size();
            t.key (36 + 1);   // FORWARD
            t.play (0.12);
            reaches.push_back (t.deck.getPositionSeconds() - before);
            t.key (36 + 1, false);
            t.play (0.3);
            // When the sound starts (the record moving: over -40 dB).
            auto start = from;
            while (start < t.out.size() && std::abs (t.out[start]) < 0.003f)
                ++start;
            starts.push_back ((double) (start - from) / sampleRate);
        }
        const auto spread = [] (const std::vector<double>& v) { return *std::max_element (v.begin(), v.end()) - *std::min_element (v.begin(), v.end()); };
        INFO ((feel ? "FEEL" : "plain") << ": starts spread " << spread (starts) * 1000.0 << " ms, reaches spread " << spread (reaches) << " s");
        if (feel)
        {
            CHECK (spread (starts) > 0.0005);
            CHECK (spread (starts) < 0.008);
            CHECK (spread (reaches) > 0.002);
        }
        else
        {
            CHECK (spread (starts) < 0.0002);
            CHECK (spread (reaches) < 1.0e-6);
        }
    }
}

TEST_CASE ("FEEL in the plugin: on for a new deck; off for projects and presets from before 0.12.0", "[feel][plugin]")
{
    ParagonX1AudioProcessor p;
    for (const auto& ids : ParamIDs::deckIds)
    {
        CHECK (p.getParameters().getRawParameterValue (ids.feel)->load() > 0.5f);
        CHECK (p.getParameters().getRawParameterValue (ids.dirt)->load() == 0.0f);
    }
    CHECK (p.getDeckSettings (0).feel);

    // A project from before 0.12.0: no deckFeel in it.
    juce::MemoryBlock state;
    p.getStateInformation (state);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);
    for (const auto& ids : ParamIDs::deckIds)
        for (auto* child = xml->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            if (child->getStringAttribute ("id") == ids.feel || child->getStringAttribute ("id") == ids.dirt)
                xml->removeChildElement (child, true);
            child = next;
        }
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary (*xml, old);
    ParagonX1AudioProcessor q;
    q.setStateInformation (old.getData(), (int) old.getSize());
    for (const auto& ids : ParamIDs::deckIds)
        CHECK (q.getParameters().getRawParameterValue (ids.feel)->load() < 0.5f);
    // And a project saved now keeps FEEL on.
    ParagonX1AudioProcessor r;
    r.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (r.getParameters().getRawParameterValue (ParamIDs::deckFeel)->load() > 0.5f);

    // A preset from before 0.12.0 holding the decks' settings: FEEL off; a new one: as saved.
    Rig before;
    before.parameters[ParamIDs::deckOn] = 1.0f;
    const auto loadedBefore = Rig::fromXml (*juce::XmlDocument::parse (before.toXml()->toString()));
    REQUIRE (loadedBefore.has_value());
    CHECK (loadedBefore->parameters.at (ParamIDs::deckFeel) == 0.0f);
    Rig now = before;
    now.parameters[ParamIDs::deckFeel] = 1.0f;
    const auto loadedNow = Rig::fromXml (*juce::XmlDocument::parse (now.toXml()->toString()));
    REQUIRE (loadedNow.has_value());
    CHECK (loadedNow->parameters.at (ParamIDs::deckFeel) == 1.0f);
}
