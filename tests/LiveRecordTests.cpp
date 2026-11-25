// THE LIVE RECORD (PR7-T 0.12.0): a deck scratching what the synth just played. What's checked: the tape keeps what
// it's given, and its slower copies, made as it records, are the ones a record would have (so it's read as cleanly at
// speed); a needle drop on it lands on a key press and plays what was played from there; a technique lands on the
// newest press far enough back; in the plugin it's the third built-in record, the synth's notes go on it, the decks
// never do, and the project keeps it.
#include "SynthTestUtils.h"

#include "PluginProcessor.h"
#include "ui/DeckPage.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinRel;

namespace
{
    float sine (double hz, std::int64_t at, float level = 0.5f)
    {
        return level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) at / sampleRate);
    }

    // A deck on THE LIVE RECORD with its tape, written block by block before the deck plays (as the processor does),
    // from `signal (clock)`; keys pressed by MIDI; the output kept (left).
    struct LiveDeck
    {
        deck::LiveTape tape;
        deck::Deck deck;
        deck::Settings settings;
        juce::MidiBuffer pending;
        std::vector<float> out;
        std::int64_t clock = 0;
        std::function<float (std::int64_t)> signal = [] (std::int64_t) { return 0.0f; };

        LiveDeck()
        {
            tape.prepare (sampleRate);
            deck.prepare (sampleRate);
            deck.setLiveTape (&tape);
            deck.seedRandom (1);
            settings.needle = 0.0f;
            settings.startSeconds = 0.05f;
        }
        void key (int note, bool down = true, float velocity = 1.0f)
        {
            pending.addEvent (down ? juce::MidiMessage::noteOn (1, note, velocity) : juce::MidiMessage::noteOff (1, note), 0);
        }
        void key (deck::Key k, bool down = true) { key (settings.firstKey + (int) k, down); }
        size_t play (double seconds)
        {
            const auto from = out.size();
            std::vector<float> in ((size_t) blockSize), left ((size_t) blockSize), right ((size_t) blockSize);
            for (int b = 0; b < (int) std::round (seconds * sampleRate / blockSize); ++b)
            {
                for (int i = 0; i < blockSize; ++i)
                    in[(size_t) i] = signal (clock + i);
                tape.write (in.data(), in.data(), blockSize, clock);
                std::fill (left.begin(), left.end(), 0.0f);
                std::fill (right.begin(), right.end(), 0.0f);
                deck.process (left.data(), right.data(), blockSize, pending, 0, deck::liveRecord().get(), settings);
                pending.clear();
                out.insert (out.end(), left.begin(), left.end());
                clock += blockSize;
            }
            return from;
        }
    };

    struct Plugin
    {
        ParagonX1AudioProcessor processor;
        Plugin()
        {
            processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
            processor.prepareToPlay (sampleRate, blockSize);
            processor.seedRandomForTest (1);
        }
        void set (const juce::String& id, float value)
        {
            auto* parameter = processor.getParameters().getParameter (id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
        // Plays `seconds`; `events` are (block number, message).
        std::vector<float> play (double seconds, std::vector<std::pair<int, juce::MidiMessage>> events = {})
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                for (const auto& [at, message] : events)
                    if (at == b)
                        midi.addEvent (message, 0);
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };

    int blockAt (double seconds) { return (int) (seconds * sampleRate / blockSize); }
}

TEST_CASE ("THE LIVE RECORD's tape: what it's given comes back, and its slower copies are a record's", "[live]")
{
    // The same five seconds of sound as a record and on the tape (from clock 0): read anywhere at any speed, the tape
    // gives what the record does, so a fast stroke on it is as clean (its copies made as it records, not afterwards).
    const auto seconds = 5.0;
    juce::AudioBuffer<float> audio (2, (int) (seconds * sampleRate));
    juce::Random random (3);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        audio.setSample (0, i, sine (440.0, i) + 0.2f * (random.nextFloat() - 0.5f) + sine (9100.0, i, 0.2f));
        audio.setSample (1, i, sine (660.0, i) + 0.2f * (random.nextFloat() - 0.5f));
    }
    const auto record = deck::makeRecord (audio, sampleRate, "check");
    deck::LiveTape tape;
    tape.prepare (sampleRate);
    for (int at = 0; at < audio.getNumSamples(); at += blockSize)
        tape.write (audio.getReadPointer (0, at), audio.getReadPointer (1, at), juce::jmin (blockSize, audio.getNumSamples() - at), at);
    CHECK (tape.getEnd() == audio.getNumSamples());
    CHECK (tape.hasSound());
    // The tape's newest readable place stays within about 20 ms of the newest sound (the slowest copy's filter).
    CHECK (tape.newest() > audio.getNumSamples() - 0.02 * sampleRate);
    CHECK (tape.newest() < audio.getNumSamples());
    for (const auto pace : { 1.0, 1.7, 2.5, 4.4, 7.0, 12.0 })
        for (const auto at : { 1.0, 2.123, 3.5, 4.2 })
            for (int c = 0; c < 2; ++c)
            {
                INFO ("pace " << pace << ", at " << at << " s, side " << c);
                const auto position = at * sampleRate + 0.37;
                CHECK_THAT (tape.read (c, position, pace), WithinAbs (deck::Deck::read (*record, c, position, pace), 1.0e-5));
            }
}

TEST_CASE ("THE LIVE RECORD's tape: twelve seconds kept, older sound gone, a jump in the clock starts it afresh", "[live]")
{
    deck::LiveTape tape;
    tape.prepare (sampleRate);
    std::vector<float> block ((size_t) blockSize, 0.25f);
    std::int64_t clock = 0;
    for (; clock < (std::int64_t) (30.0 * sampleRate); clock += blockSize)
        tape.write (block.data(), nullptr, blockSize, clock);
    const auto kept = ((double) tape.getEnd() - tape.oldest()) / sampleRate;
    CHECK (kept >= deck::LiveTape::seconds);
    CHECK (kept < deck::LiveTape::seconds + 12.0);   // (the ring rounds up to a power of two)
    // A key press older than what's kept isn't offered.
    tape.addHit (clock - (std::int64_t) (25.0 * sampleRate));
    CHECK (tape.hit (0, 0.0) < 0.0);
    tape.addHit (clock - (std::int64_t) (1.0 * sampleRate));
    CHECK (tape.hit (0, 0.0) == (double) (clock - (std::int64_t) sampleRate));
    CHECK (tape.hit (0, 1.5 * sampleRate) < 0.0);   // not far enough back
    // The clock jumps back (the host started again): the tape starts again from there, with no presses.
    tape.write (block.data(), nullptr, blockSize, 0);
    CHECK (tape.getEnd() == blockSize);
    CHECK (tape.hit (0, 0.0) < 0.0);
}

TEST_CASE ("THE LIVE RECORD: DROP 1 lands on the newest key press and plays what was played from there", "[live]")
{
    // A second of silence, then a 440 Hz tone from the press (at 1 s), stopping at 1.5 s, then silence: two seconds
    // later DROP 1 plays it again, starting on the press.
    LiveDeck t;
    const auto press = (std::int64_t) (1.0 * sampleRate);
    t.signal = [press] (std::int64_t at) { return at >= press && at < press + (std::int64_t) (0.5 * sampleRate) ? sine (440.0, at) : 0.0f; };
    t.play (0.9);
    t.tape.addHit (press);
    t.play (2.6);
    CHECK (rms (t.out, 0, t.out.size()) < 1.0e-6);   // the deck silent: the motor off
    t.key (deck::Key::drop1);
    const auto from = t.play (1.0);
    // The drop lands 4 ms before the press: silence, then the tone at its level and pitch, for half a second.
    CHECK (rms (t.out, from, from + (size_t) (0.003 * sampleRate)) < 0.01);
    CHECK_THAT (rms (t.out, from + (size_t) (0.1 * sampleRate), from + (size_t) (0.4 * sampleRate)), WithinAbs (0.5 / std::sqrt (2.0), 0.01));
    CHECK_THAT (frequencyOf (t.out, from + (size_t) (0.1 * sampleRate), from + (size_t) (0.4 * sampleRate)), WithinAbs (440.0, 1.0));
    CHECK (rms (t.out, from + (size_t) (0.56 * sampleRate), from + (size_t) (0.9 * sampleRate)) < 0.01);
    // It follows the tone exactly (the needle on the tape's own samples, a whole number behind).
    const auto behind = t.deck.getLiveBehindSeconds();
    CHECK (behind > 2.4);
    CHECK (behind < 2.8);
}

TEST_CASE ("THE LIVE RECORD: a technique lands on the newest key press far enough back, and scratches it", "[live]")
{
    // A tone struck at 1 s; at 1.55 s a second press (too fresh, 0.15 s old, for a stroke of 0.27 s). A BABY lands
    // on the first: the stroke pushes the tone forwards and back.
    LiveDeck t;
    const auto press = (std::int64_t) (1.0 * sampleRate);
    t.signal = [press] (std::int64_t at) { return at >= press && at < press + (std::int64_t) (0.8 * sampleRate) ? sine (440.0, at) : 0.0f; };
    t.play (1.5);
    t.tape.addHit (press);
    t.tape.addHit (press + (std::int64_t) (0.55 * sampleRate));
    t.play (0.2);
    REQUIRE (t.tape.hit (0, 0.3 * sampleRate) == (double) press);
    t.key (t.settings.firstScratchKey + 0);   // BABY
    const auto from = t.play (0.2);           // (a BABY is half a beat: 0.25 s at 120 BPM)
    t.key (t.settings.firstScratchKey + 0, false);   // let go before it's through: it finishes the stroke it's on
    t.play (0.4);
    CHECK (t.deck.getScratch() == -1);
    // The tone heard while the record moved (louder as it moves faster: the cartridge), silent where the hand turned.
    const auto heard = rms (t.out, from, from + (size_t) (0.25 * sampleRate));   // (through the release)
    CHECK (heard > 0.05);
    // Back where it started: the press, 4 ms before it.
    const auto at = (double) t.tape.getEnd() - t.deck.getLiveBehindSeconds() * sampleRate;
    CHECK_THAT (at, WithinAbs ((double) press - 0.004 * sampleRate, 0.003 * sampleRate));

    // Before anything's played: nothing to scratch, and a technique is silent.
    LiveDeck empty;
    empty.play (0.5);
    empty.key (empty.settings.firstScratchKey + 0);
    const auto start = empty.play (0.25);
    CHECK (rms (empty.out, start, empty.out.size()) < 1.0e-6);
}

TEST_CASE ("THE LIVE RECORD in the plugin: the third built-in record, the synth's notes on it, never a deck", "[live][plugin]")
{
    // BUILT IN: HOUSE, NU, LIVE, HOUSE again; the screen says how far behind the synth the needle is.
    {
        Plugin u;
        DeckPage page (u.processor);
        page.setSize (1476, 672);
        auto* builtIn = dynamic_cast<juce::Button*> (page.getRack().findChildWithID ("deckHouse"));
        REQUIRE (builtIn != nullptr);
        builtIn->onClick();
        builtIn->onClick();
        CHECK (u.processor.getDeckRecord (0) == deck::liveRecord());
        CHECK (u.processor.getBuiltInRecord (0) == 2);
        page.refreshForTest();
        CHECK (page.getRack().getScreen().record == "LIVE");
        CHECK (page.getRack().getScreen().line1.contains ("BEHIND THE SYNTH"));
        builtIn->onClick();
        CHECK (u.processor.getDeckRecord (0) == deck::houseRecord());
    }

    // A synth note (C4, the deck's keys are lower) held 1.4 s; then DROP 1 plays it again from the deck (the motor
    // starting as a turntable's does, over START's 0.7 s: measured once it's up to speed).
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    p.processor.useBuiltInRecord (2, 0);
    const auto c4 = 60, drop1 = 24 + (int) deck::Key::drop1;
    const auto out = p.play (3.5, { { blockAt (0.2), juce::MidiMessage::noteOn (1, c4, 1.0f) },
                                    { blockAt (1.6), juce::MidiMessage::noteOff (1, c4) },
                                    { blockAt (2.0), juce::MidiMessage::noteOn (1, drop1, 1.0f) },
                                    { blockAt (2.05), juce::MidiMessage::noteOff (1, drop1) } });
    // The same without the drop: what the synth's release leaves there.
    Plugin q;
    q.set (ParamIDs::deckOn, 1.0f);
    q.processor.useBuiltInRecord (2, 0);
    const auto plain = q.play (3.5, { { blockAt (0.2), juce::MidiMessage::noteOn (1, c4, 1.0f) },
                                      { blockAt (1.6), juce::MidiMessage::noteOff (1, c4) } });
    const auto note = rms (out, (size_t) (0.3 * sampleRate), (size_t) (0.6 * sampleRate));
    const auto replay = rms (out, (size_t) (2.9 * sampleRate), (size_t) (3.2 * sampleRate));
    const auto without = rms (plain, (size_t) (2.9 * sampleRate), (size_t) (3.2 * sampleRate));
    INFO ("note " << note << ", replayed " << replay << ", without " << without);
    REQUIRE (note > 0.01);
    CHECK (replay > 0.3 * note);
    CHECK (replay > 10.0 * without + 1.0e-4);
    CHECK_THAT (frequencyOf (out, (size_t) (2.9 * sampleRate), (size_t) (3.2 * sampleRate)),
                WithinRel (frequencyOf (out, (size_t) (0.3 * sampleRate), (size_t) (0.6 * sampleRate)), 0.01));

    // A deck playing a record isn't put on the tape: with only THE HOUSE RECORD playing on deck A, the tape is silent.
    Plugin r;
    r.set (ParamIDs::deckOn, 1.0f);
    r.play (2.0, { { 0, juce::MidiMessage::noteOn (1, 24 + (int) deck::Key::play, 1.0f) } });
    float loudest = 0.0f;
    for (int b = 0; b < deck::LiveTape::bins; ++b)
        loudest = juce::jmax (loudest, r.processor.getLiveTape().getBin (b));
    CHECK (r.processor.getDeck (0).getSpeed() > 0.9f);
    CHECK (loudest == 0.0f);

    // The project keeps it.
    juce::MemoryBlock state;
    p.processor.getStateInformation (state);
    Plugin restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getDeckRecord (0) == deck::liveRecord());
    CHECK (restored.processor.getBuiltInRecord (0) == 2);
}
