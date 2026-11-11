// THE DECK (PR7-T 0.2.0): the turntable, on its own and in the plugin. What's checked is what it does to the sound:
// a record at normal speed comes out as it went in; the pitch fader, 33 / 45, the motor's START and BRAKE, BACKSPIN,
// HOLD and the pitch wheel's hand move its speed (and so its pitch) as a turntable's would; its loudness follows its
// speed; it's clean when fast (no aliasing); a needle drop lands where its cue says, without a click; and in the
// plugin it's off by default (the synth's presets unchanged), on it takes its keys and plays through the output.
#include "SynthTestUtils.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/MainView.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    // A record of a steady sine, `seconds` long, at `hz`, peak `level` (at the host's rate, 48 kHz).
    std::shared_ptr<const deck::Record> sineRecord (double hz, double seconds = 10.0, float level = 0.5f)
    {
        juce::AudioBuffer<float> audio (1, (int) (seconds * sampleRate));
        for (int i = 0; i < audio.getNumSamples(); ++i)
            audio.setSample (0, i, level * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / sampleRate));
        return deck::makeRecord (audio, sampleRate, "sine");
    }

    // A deck playing a record, with its settings, its keys pressed by MIDI, its output kept (left).
    struct Turntable
    {
        deck::Deck deck;
        deck::Settings settings;
        std::shared_ptr<const deck::Record> record;
        juce::MidiBuffer pending;
        std::vector<float> out;

        explicit Turntable (std::shared_ptr<const deck::Record> r) : record (std::move (r))
        {
            deck.prepare (sampleRate);
            deck.seedRandom (1);
            settings.needle = 0.0f;   // the needle's thump and crackle off, unless a test wants them
        }
        void key (deck::Key k, bool down = true, float velocity = 1.0f)
        {
            const auto note = settings.firstKey + (int) k;
            pending.addEvent (down ? juce::MidiMessage::noteOn (1, note, velocity) : juce::MidiMessage::noteOff (1, note), 0);
        }
        void wheel (float position)   // -1 to 1
        {
            pending.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, 8192 + juce::roundToInt (position * 8191.0f))), 0);
        }
        // Plays `seconds`, returning where that piece starts in `out`.
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
        // The level and pitch of the last `seconds` played.
        double lastRms (double seconds) const { return rms (out, out.size() - (size_t) (seconds * sampleRate), out.size()); }
        double lastHz (double seconds) const { return frequencyOf (out, out.size() - (size_t) (seconds * sampleRate), out.size()); }
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
        std::vector<float> play (double seconds, juce::MidiBuffer first = {})
        {
            std::vector<float> out;
            juce::AudioBuffer<float> buffer (2, blockSize);
            for (int b = 0; b < (int) (seconds * sampleRate / blockSize); ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;
                if (b == 0)
                    midi = first;
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            }
            return out;
        }
    };

    juce::MidiBuffer noteOn (int note)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);
        return midi;
    }
}

TEST_CASE ("THE DECK at normal speed: the record comes out as it went in, its level and pitch exact", "[deck]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.5);
    CHECK (t.deck.getSpeed() == 1.0f);
    CHECK_THAT (t.lastHz (0.2), WithinAbs (1000.0, 0.5));
    CHECK_THAT (t.lastRms (0.2), WithinAbs (0.5 / std::sqrt (2.0), 0.002));

    // Where it is, it plays the record's sine as it is there (between its samples: the motor's ramp left the needle
    // part way between two), to within 0.0005 (-66 dB under the sine).
    const auto end = t.deck.getPositionSeconds() * sampleRate;   // the last sample was read here
    for (int k = 0; k < 256; ++k)
        CHECK_THAT (t.out[t.out.size() - 1 - (size_t) k],
                    WithinAbs (0.5 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (end - k) / sampleRate), 5.0e-4));

    // And read on the record's own samples at normal speed, the interpolator gives them back exactly.
    const auto& top = t.record->levels[0];
    for (int i = 0; i < 64; ++i)
        CHECK_THAT (deck::Deck::read (*t.record, 0, 1000.0 + i, 1.0), WithinAbs (top.getSample (0, deck::Record::padding + 1000 + i), 1.0e-6));
}

TEST_CASE ("THE DECK: the pitch fader and 45 RPM move the pitch as the speed; a record at 0 is silent", "[deck]")
{
    for (const auto& [pitch, speed45, expected] : { std::tuple { 0.08f, false, 1080.0 }, std::tuple { -0.16f, false, 840.0 },
                                                    std::tuple { 0.0f, true, 1350.0 } })
    {
        Turntable t (sineRecord (1000.0));
        t.settings.pitch = pitch;
        t.settings.speed45 = speed45;
        t.settings.startSeconds = 0.05f;
        t.key (deck::Key::play);
        t.play (0.5);
        INFO ("pitch " << pitch << ", 45: " << speed45);
        CHECK_THAT (t.lastHz (0.2), WithinAbs (expected, 1.0));
        // Normal play at any pitch is at the record's own level (the cartridge's loudness is relative to the motor).
        CHECK_THAT (t.lastRms (0.2), WithinAbs (0.5 / std::sqrt (2.0), 0.003));
    }
    Turntable stopped (sineRecord (1000.0));
    stopped.play (0.3);
    CHECK (stopped.lastRms (0.3) == 0.0);
}

TEST_CASE ("THE DECK's motor: PLAY comes up to speed over START, STOP winds down over BRAKE, as straight lines", "[deck]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 1.0f;
    t.settings.brakeSeconds = 2.0f;
    t.key (deck::Key::play);
    t.play (0.5);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (0.5, 0.04));   // half way up (the slipmat a few ms behind)
    t.play (0.7);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (1.0, 1.0e-4));

    t.key (deck::Key::stop);
    t.play (1.0);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (0.5, 0.04));
    // Half speed: the pitch an octave down, and quieter (the level follows the speed).
    CHECK_THAT (t.lastHz (0.04), WithinAbs (500.0, 30.0));
    t.play (1.2);
    CHECK (std::abs (t.deck.getSpeed()) < 1.0e-4f);
    CHECK (t.lastRms (0.1) < 1.0e-4);
}

TEST_CASE ("THE DECK's BACKSPIN: thrown back at SPIN times the speed (the pitch up, louder), slowing to a stop", "[deck]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.settings.spinSpeed = 2.0f;
    t.settings.spinSeconds = 4.0f;
    t.key (deck::Key::play);
    t.play (3.0);
    const auto before = t.deck.getPositionSeconds();
    t.key (deck::Key::backspin);
    t.play (0.15);
    CHECK (t.deck.getSpeed() < -1.8f);
    CHECK (t.deck.getPositionSeconds() < before);   // going backwards
    // Backwards at about twice the speed: the sine's pitch doubled, and its level about doubled (+6 dB, the cap).
    CHECK_THAT (t.lastHz (0.05), WithinAbs (1900.0, 120.0));
    CHECK_THAT (db (t.lastRms (0.05) / (0.5 / std::sqrt (2.0))), WithinAbs (5.6, 0.8));
    t.play (4.3);   // (it reaches 0 at 4 s, the record settling about 90 ms after: the flick's 20 ms lag)
    CHECK (t.deck.getSpeed() == 0.0f);
    CHECK_FALSE (t.deck.isMotorOn());
    CHECK (t.lastRms (0.1) == 0.0);
    // PLAY starts it again, forwards.
    t.key (deck::Key::play);
    t.play (0.3);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (1.0, 1.0e-4));
}

TEST_CASE ("THE DECK's HOLD: a hand stops the record (silent at once); let go, it catches the platter in ms", "[deck]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.5);
    t.key (deck::Key::hold);
    t.play (0.05);
    CHECK (std::abs (t.deck.getSpeed()) < 0.01f);
    CHECK (t.lastRms (0.02) < 0.005);
    const auto held = t.deck.getPositionSeconds();
    t.play (0.3);
    CHECK_THAT (t.deck.getPositionSeconds(), WithinAbs (held, 0.001));
    // Let go: the motor never stopped, so the record is back at full speed within about 100 ms.
    t.key (deck::Key::hold, false);
    t.play (0.1);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (1.0, 0.01));
}

TEST_CASE ("THE DECK's hand on the pitch wheel: moves the record by REACH and back (a baby scratch), then lets go", "[deck]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.settings.reachTurns = 0.25f;
    t.key (deck::Key::play);
    t.play (1.0);
    const auto grabbed = t.deck.getPositionSeconds();
    // All the way up: the record pushed forward a quarter turn (0.45 s), then held there, silent.
    t.wheel (1.0f);
    t.play (0.2);
    CHECK_THAT (t.deck.getPositionSeconds() - grabbed, WithinAbs (0.25 * deck::secondsPerTurn, 0.003));
    CHECK (t.lastRms (0.05) < 1.0e-3);
    // Pulled back to the centre: the record back where the hand took it, and (the wheel left there 50 ms) let go,
    // the motor taking it on from there.
    t.wheel (0.0f);
    t.play (0.04);
    CHECK_THAT (t.deck.getPositionSeconds() - grabbed, WithinAbs (0.0, 0.01));
    t.play (0.4);   // (let go about 7 ms in; the slipmat's 20 ms catching up nearly done by 0.3 s)
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (1.0, 0.001));
    CHECK_THAT (t.lastHz (0.1), WithinAbs (1000.0, 1.0));

    // A scratch: the wheel swept up and back over 0.1 s each way. Forward the pitch goes up, back it comes down
    // backwards; both loud while moving fast.
    Turntable s (sineRecord (1000.0));
    s.settings.reachTurns = 0.25f;   // 0.45 s of record in 0.1 s: 4.5 times the speed at its fastest-ish
    for (int step = 1; step <= 20; ++step)
    {
        s.wheel ((float) step / 20.0f);
        s.play (0.005);
    }
    CHECK (s.deck.getSpeed() > 2.0f);
    CHECK (s.lastRms (0.02) > 0.5);
}

TEST_CASE ("THE DECK is clean when fast: the record's highs never fold back down (the filtered copies, the kernel)", "[deck]")
{
    // A 15 kHz tone read at twice its speed would be 30 kHz: past 24 kHz (the host's Nyquist), it must be filtered
    // away, not folded down to 18 kHz. A 5 kHz tone at twice the speed (10 kHz) must pass at its full level.
    for (const auto& [hz, rate, level] : { std::tuple { 15000.0, 2.0, 0.0 }, std::tuple { 15000.0, 3.3, 0.0 },
                                           std::tuple { 5000.0, 2.0, 1.0 }, std::tuple { 3000.0, 3.3, 1.0 },
                                           std::tuple { 1000.0, 0.37, 1.0 }, std::tuple { 7000.0, 1.5, 1.0 } })
    {
        const auto record = sineRecord (hz, 4.0, 1.0f);
        std::vector<float> out (8192);
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = deck::Deck::read (*record, 0, 1000.0 + rate * (double) i, rate);
        INFO (hz << " Hz at " << rate << " times: " << db (rms (out, 0, out.size()) * std::sqrt (2.0)) << " dB");
        if (level > 0.0)
            CHECK_THAT (rms (out, 0, out.size()) * std::sqrt (2.0), WithinAbs (1.0, 0.03));
        else
            CHECK (db (rms (out, 0, out.size()) * std::sqrt (2.0)) < -60.0);
    }
}

TEST_CASE ("THE DECK's needle drop: lands on its cue, the old place faded out under it (no click); NEEDLE's thump", "[deck]")
{
    Turntable t (sineRecord (1000.0, 8.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.5);
    const auto from = t.out.size();
    t.key (deck::Key::drop4);
    t.play (0.1);
    CHECK_THAT (t.deck.getPositionSeconds(), WithinAbs (0.75 * 8.0 + 0.1, 0.003));   // (0.1 s is 19 blocks: 0.1013 s)
    // A 1 kHz sine at 0.5 never moves more than 0.066 a sample, and while two places cross over, a little more (their
    // difference times 1/192 a sample: at most 0.071). A jump straight to another place in its cycle could be up to 1.
    float biggest = 0.0f;
    for (auto i = from; i < t.out.size(); ++i)
        biggest = juce::jmax (biggest, std::abs (t.out[i] - t.out[i - 1]));
    CHECK (biggest < 0.08f);

    // With NEEDLE up: the landing's low thump (55 Hz) comes in with it, then dies away.
    Turntable n (sineRecord (1000.0, 8.0, 0.0f));   // a silent record: only the needle heard
    n.settings.needle = 1.0f;
    const auto at = n.play (0.1);
    CHECK (rms (n.out, at, n.out.size()) == 0.0);
    n.key (deck::Key::drop1);
    const auto landed = n.play (0.06);
    CHECK (levelAt (n.out, 55.0, landed, (size_t) (0.06 * sampleRate)) > 0.05);
    n.play (1.5);
    CHECK (n.lastRms (0.2) < 1.0e-4);
}

TEST_CASE ("THE HOUSE RECORD: 8 s, a sound at each cue, its tone at 1 kHz", "[deck]")
{
    const auto record = deck::houseRecord();
    REQUIRE (record != nullptr);
    CHECK (record == deck::houseRecord());   // made once
    CHECK_THAT (record->seconds(), WithinAbs (8.0, 1.0e-9));
    std::vector<float> audio (record->levels[0].getReadPointer (0) + deck::Record::padding,
                              record->levels[0].getReadPointer (0) + deck::Record::padding + record->length);
    for (const auto cue : { 0.0, 2.0, 4.0, 6.0 })
    {
        INFO ("the sound at " << cue << " s");
        CHECK (rms (audio, (size_t) (cue * sampleRate), (size_t) ((cue + 0.4) * sampleRate)) > 0.05);
        CHECK (record->levels[0].getMagnitude (0, deck::Record::padding + (int) (cue * sampleRate), (int) (2.0 * sampleRate)) < 0.75f);
    }
    CHECK_THAT (frequencyOf (audio, (size_t) (6.2 * sampleRate), (size_t) (7.4 * sampleRate)), WithinAbs (deck::houseToneHz, 0.1));
}

TEST_CASE ("THE DECK in the plugin: off by default, its keys the synth's; on, they're the deck's and it plays out", "[deck][plugin]")
{
    // Off (the default): note 24 (the deck's PLAY when it's on) plays the synth, as on PR7-K.
    Plugin off;
    CHECK_FALSE (off.processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() > 0.5f);
    const auto synth = off.play (0.3, noteOn (24));
    CHECK (rms (synth, 0, synth.size()) > 1.0e-3);

    // On: the same key starts the deck's motor and plays no synth note (with the deck itself silent, nothing at all
    // comes out); DROP 4 plays THE HOUSE RECORD's 1 kHz tone.
    Plugin on;
    on.set (ParamIDs::deckOn, 1.0f);
    on.set (ParamIDs::deckStart, 0.05f);
    on.set (ParamIDs::deckNeedle, 0.0f);
    on.set (ParamIDs::deckLevel, -60.0f);
    const auto started = on.play (0.25, noteOn (24));
    CHECK (on.processor.getDeck().isMotorOn());
    CHECK (rms (started, 0, started.size()) == 0.0);
    on.set (ParamIDs::deckLevel, 0.0f);
    const auto tone = on.play (0.6, noteOn (24 + (int) deck::Key::drop4));
    CHECK_THAT (on.processor.getDeck().getPositionSeconds(), WithinAbs (6.0 + 0.6, 0.01));
    CHECK_THAT (frequencyOf (tone, (size_t) (0.2 * sampleRate), tone.size()), WithinAbs (1000.0, 0.5));
    // Through the plugin's OUTPUT (-6 dB by default) and the rest, at the record's level: about -9 dB RMS.
    CHECK (rms (tone, (size_t) (0.2 * sampleRate), tone.size()) > 0.1);

    // KEYS moves the block: from C2 (36), note 24 is the synth's again.
    on.set (ParamIDs::deckKeys, 2.0f);
    on.set (ParamIDs::deckLevel, -60.0f);   // the deck silent, so what's heard is the synth
    const auto moved = on.play (0.3, noteOn (24));
    CHECK (rms (moved, 0, moved.size()) > 1.0e-3);
}

TEST_CASE ("THE DECK's record is kept with the project: a file loaded comes back; a missing one says so", "[deck][plugin]")
{
    const auto folder = juce::File::createTempFile ("pr7t-deck");
    folder.createDirectory();
    const auto file = folder.getChildFile ("my break.wav");
    {
        juce::AudioBuffer<float> audio (2, 24000);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                audio.setSample (c, i, 0.25f * (float) std::sin (0.05 * i));
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        REQUIRE (stream != nullptr);
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (2).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    Plugin p;
    CHECK (p.processor.getDeckRecord() == deck::houseRecord());
    REQUIRE (p.processor.loadDeckRecord (file));
    CHECK (p.processor.getDeckRecord()->name == "my break");
    CHECK_THAT (p.processor.getDeckRecord()->seconds(), WithinAbs (24000.0 / 44100.0, 1.0e-9));
    juce::MemoryBlock state;
    p.processor.getStateInformation (state);

    Plugin restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getDeckRecordFile() == file.getFullPathName());
    CHECK (restored.processor.getDeckRecord()->name == "my break");
    CHECK (restored.processor.getDeckRecordProblem().isEmpty());

    // The file gone: the project still names it, the deck plays the house record meanwhile, and says why.
    file.deleteFile();
    Plugin missing;
    missing.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (missing.processor.getDeckRecordFile() == file.getFullPathName());
    CHECK (missing.processor.getDeckRecord() == deck::houseRecord());
    CHECK (missing.processor.getDeckRecordProblem().contains ("missing"));

    // A project with no record named: the house record.
    Plugin fresh;
    fresh.processor.getStateInformation (state);
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getDeckRecord() == deck::houseRecord());
    CHECK (restored.processor.getDeckRecordFile().isEmpty());
    folder.deleteRecursively();
}

TEST_CASE ("THE DECK's page: its tab, its keys press the deck's, the platter's hand scratches", "[deck][ui]")
{
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    p.set (ParamIDs::deckStart, 0.05f);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.processor.createEditor());
    auto* view = dynamic_cast<MainView*> (editor->getChildComponent (0));
    REQUIRE (view != nullptr);
    auto* tab = dynamic_cast<juce::Button*> (view->findChildWithID ("deckTab"));
    REQUIRE (tab != nullptr);
    tab->onClick();
    CHECK (view->getDeckPage().isVisible());

    // The page's PLAY presses the deck's key: the motor starts.
    auto& page = view->getDeckPage();
    juce::TextButton* play = nullptr;
    for (auto* child : page.getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "PLAY")
            play = b;
    REQUIRE (play != nullptr);
    REQUIRE (play->isEnabled());   // (off while the deck is: they'd play the synth)
    play->onClick();
    p.play (0.3);
    CHECK (p.processor.getDeck().isMotorOn());
    CHECK_THAT (p.processor.getDeck().getSpeed(), WithinAbs (1.0, 0.01));

    // The mouse on the platter: held still, the record stops; a quarter turn clockwise moves it 0.45 s on.
    auto& platter = page.getPlatter();
    const auto centre = platter.getLocalBounds().getCentre().toFloat();
    const auto r = (float) platter.getWidth() * 0.35f;
    const auto mouse = [&platter] (juce::Point<float> at)
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &platter, &platter, juce::Time::getCurrentTime(), at,
                                 juce::Time::getCurrentTime(), 1, false);
    };
    platter.mouseDown (mouse (centre.translated (0.0f, -r)));
    p.play (0.2);
    const auto held = p.processor.getDeck().getPositionSeconds();
    CHECK (std::abs (p.processor.getDeck().getSpeed()) < 0.01f);
    for (int step = 1; step <= 10; ++step)
    {
        const auto angle = juce::MathConstants<float>::halfPi * (float) step / 10.0f;
        platter.mouseDrag (mouse (centre + juce::Point<float> (r * std::sin (angle), -r * std::cos (angle))));
    }
    p.play (0.2);
    CHECK_THAT (p.processor.getDeck().getPositionSeconds() - held, WithinAbs (0.25 * deck::secondsPerTurn, 0.01));
    platter.mouseUp (mouse (centre.translated (r, 0.0f)));
    p.play (0.2);
    CHECK_THAT (p.processor.getDeck().getSpeed(), WithinAbs (1.0, 0.01));
}
