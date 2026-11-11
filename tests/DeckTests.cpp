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
                if (settings.ppq >= 0.0)   // the song playing: on to the next block's place, as a host's is
                    settings.ppq += blockSize / sampleRate * settings.bpm / 60.0;
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
    const auto centre = platter.getPlatterCentre();
    const auto r = platter.getRecordRadius() * 0.7f;
    CHECK (platter.getLocalBounds().toFloat().contains (centre));
    CHECK (platter.getRecordRadius() > 100.0f);   // the record big enough to grab (at the plugin's 1x), the rack shown
    {
        // HIDE RACK (0.4.0): the turntable gets the page's height.
        const auto withRack = platter.getRecordRadius();
        page.setRackShown (false);
        CHECK_FALSE (page.getRack().isVisible());
        CHECK (platter.getRecordRadius() > withRack * 1.4f);
        page.setRackShown (true);
        CHECK (page.getRack().isVisible());
    }
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

TEST_CASE ("THE TURNTABLE's pictures: built in, the same shape, in the family's colours", "[deck][ui][colours]")
{
    // The Tron look keeps to the family's hues (pink through red to orange, 318-42 degrees), as PR7-D's kit, PR7-I's
    // guitar and PR7-O's strings do: the render's tone mapping and glow would drift towards yellow and purple.
    const auto pictures = DeckPlatter::picturesForTest();
    REQUIRE (pictures.size() == 4);
    for (const auto& picture : pictures)
        REQUIRE (picture.isValid());
    CHECK (pictures[0].getBounds() == pictures[1].getBounds());
    CHECK_THAT (pictures[0].getWidth() / (double) pictures[0].getHeight(), WithinAbs (DeckPlatter::pictureAspect, 1.0e-3));
    const char* names[] = { "the deck", "the deck lit", "the platter", "the arm" };
    for (size_t k = 0; k < pictures.size(); ++k)
    {
        int counted = 0, outside = 0;
        for (int y = 0; y < pictures[k].getHeight(); y += 2)
            for (int x = 0; x < pictures[k].getWidth(); x += 2)
                if (const auto c = pictures[k].getPixelAt (x, y); c.getSaturation() > 0.3f && c.getBrightness() > 0.15f && c.getAlpha() > 128)
                {
                    ++counted;
                    if (const auto h = c.getHue() * 360.0f; h > 42.0f && h < 318.0f)
                        ++outside;
                }
        INFO (names[k] << ": " << outside << " of " << counted << " coloured pixels outside the family's hues");
        CHECK (counted > 100);
        CHECK (outside == 0);
    }
}

TEST_CASE ("THE TURNTABLE: the platter turns with the record, the arm swings in as it plays, the neon lights", "[deck][ui]")
{
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    p.set (ParamIDs::deckStart, 0.05f);
    DeckPage page (p.processor);
    page.setSize (1476, 672);
    auto& platter = page.getPlatter();
    page.refreshForTest();
    CHECK_THAT (platter.getArmAngle(), WithinAbs (0.0, 1.0e-4));   // at the record's start: as rendered
    const auto still = platter.createComponentSnapshot (platter.getLocalBounds(), true, 1.0f);
    // The arm is drawn (opaque): halfway down it, from its pivot (1095, 277.5) to its stylus (841.7, 793.7) in the
    // 2x picture (deck.json), its neon strip glows. Mapped onto the page through the record's centre and radius.
    {
        const auto k = platter.getRecordRadius() / 381.0f;
        const auto origin = platter.getPlatterCentre() - juce::Point<float> (570.0f, 550.0f) * k;
        const auto mid = (origin + juce::Point<float> ((1095.0f + 841.7f) * 0.5f, (277.5f + 793.7f) * 0.5f) * k).roundToInt();
        juce::uint8 brightest = 0;
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx)
                brightest = std::max (brightest, still.getPixelAt (mid.x + dx, mid.y + dy).getRed());
        CHECK (brightest > 150);
    }

    // DROP 4 (75% through the house record) and play: the arm has swung in towards the spindle (clockwise on screen,
    // round its pivot at the top right), the platter has turned, the lit neon is showing.
    p.play (0.5, noteOn (24 + (int) deck::Key::drop4));
    page.refreshForTest();
    // (The needle 81% through, at radius 190.4 px of the 2x picture: the arm 0.3092 rad in, worked out from deck.json.)
    // (The motor's 50 ms START and the slipmat leave the needle about 45 ms behind that: 0.002 rad less.)
    CHECK_THAT (platter.getArmAngle(), WithinAbs (0.3092, 0.004));
    const auto playing = platter.createComponentSnapshot (platter.getLocalBounds(), true, 1.0f);
    int changed = 0;
    for (int y = 0; y < still.getHeight(); y += 3)
        for (int x = 0; x < still.getWidth(); x += 3)
            changed += still.getPixelAt (x, y) != playing.getPixelAt (x, y) ? 1 : 0;
    INFO (changed << " sampled pixels changed");
    CHECK (changed > 2000);
}

//==============================================================================
// THE CROSSFADER (0.4.0, stage 3).
TEST_CASE ("THE CROSSFADER's curve: smooth is a quarter sine (-3 dB at the middle), CUT opens by 1%; HAMSTER turns it round", "[deck][fader]")
{
    CHECK (deck::faderGain (1.0f, 1.0f, false) == 1.0f);
    CHECK (deck::faderGain (0.0f, 1.0f, false) == 0.0f);
    CHECK (deck::faderGain (1.0f, 0.0f, false) == 1.0f);
    CHECK (deck::faderGain (0.0f, 0.0f, false) == 0.0f);
    CHECK_THAT (db (deck::faderGain (0.5f, 0.0f, false)), WithinAbs (-3.01, 0.01));   // smooth: -3 dB at the middle
    CHECK (deck::faderGain (0.5f, 1.0f, false) == 1.0f);                               // a scratch cut: open long before
    CHECK (deck::faderGain (0.01f, 1.0f, false) == 1.0f);                              // ...by 1% of the travel
    CHECK_THAT (deck::faderGain (0.005f, 1.0f, false), WithinAbs (std::sin (juce::MathConstants<double>::pi / 4.0), 1.0e-5));
    CHECK (deck::faderGain (0.0f, 1.0f, true) == 1.0f);                                // HAMSTER: the other way round
    CHECK (deck::faderGain (1.0f, 1.0f, true) == 0.0f);
    float last = 0.0f;   // never closing as it opens
    for (int k = 0; k <= 100; ++k)
    {
        const auto g = deck::faderGain ((float) k / 100.0f, 0.5f, false);
        CHECK (g >= last);
        last = g;
    }
}

TEST_CASE ("THE CROSSFADER in the deck: open is the record as it was, cut is silence, the moves are 1 ms ramps (no click)", "[deck][fader]")
{
    // What the deck plays for 0.5 s with the fader at `fader`.
    const auto playing = [] (float fader, float curve = 1.0f)
    {
        Turntable t (sineRecord (1000.0));
        t.settings.startSeconds = 0.05f;
        t.settings.fader = fader;
        t.settings.curve = curve;
        t.key (deck::Key::play);
        t.play (0.5);
        return t.out;
    };
    const auto lastRmsOf = [] (const std::vector<float>& out, double seconds) { return rms (out, out.size() - (size_t) (seconds * sampleRate), out.size()); };
    // Open (the default): sample for sample what 0.2.0's deck played.
    Turntable before (sineRecord (1000.0));
    before.settings.startSeconds = 0.05f;
    before.key (deck::Key::play);
    before.play (0.5);
    CHECK (playing (1.0f) == before.out);
    CHECK (lastRmsOf (playing (0.0f), 0.4) == 0.0);
    CHECK_THAT (db (lastRmsOf (playing (0.5f, 0.0f), 0.2) / before.lastRms (0.2)), WithinAbs (-3.01, 0.05));

    // Cut while it plays: silent within 1 ms, and no sample jumps further than the sine itself moves plus the ramp.
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.5);
    const auto from = t.out.size();
    t.settings.fader = 0.0f;
    t.play (0.1);
    CHECK (rms (t.out, from + (size_t) (0.0011 * sampleRate), t.out.size()) == 0.0);
    float biggest = 0.0f;
    for (auto i = from; i < t.out.size(); ++i)
        biggest = juce::jmax (biggest, std::abs (t.out[i] - t.out[i - 1]));
    CHECK (biggest < 0.08f);   // a cut in one sample could jump by 0.5
}

TEST_CASE ("THE CROSSFADER's CUT key flips it while held: open to cut, or (the fader closed) a tap that opens it", "[deck][fader]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.5);
    t.key (deck::Key::cut);
    t.play (0.1);
    CHECK (t.lastRms (0.05) == 0.0);
    CHECK (t.deck.getFaderGain() == 0.0f);
    t.key (deck::Key::cut, false);
    t.play (0.1);
    CHECK_THAT (t.lastRms (0.05), WithinAbs (0.5 / std::sqrt (2.0), 0.003));

    t.settings.fader = 0.0f;   // closed: CUT taps it open
    t.play (0.1);
    CHECK (t.lastRms (0.05) == 0.0);
    t.key (deck::Key::cut);
    t.play (0.1);
    CHECK_THAT (t.lastRms (0.05), WithinAbs (0.5 / std::sqrt (2.0), 0.003));
}

TEST_CASE ("THE CROSSFADER's TRANSFORM chops in time: 1/16 at 120 BPM opens 8 times a second, on the song's beat", "[deck][fader]")
{
    // Counts the openings in `out` from `from`: runs of sound after a run of silence (it was open when TRANSFORM went
    // down, so the sound at the press isn't one).
    const auto openings = [] (const std::vector<float>& out, size_t from)
    {
        int count = 0;
        bool sounding = true;
        for (auto i = from; i + 48 <= out.size(); i += 48)   // 1 ms windows
        {
            const auto now = rms (out, i, i + 48) > 0.01;
            if (now && ! sounding)
                ++count;
            sounding = now;
        }
        return count;
    };
    for (const auto& [rate, expected] : { std::pair { 1, 8 }, std::pair { 0, 4 }, std::pair { 2, 16 }, std::pair { 4, 12 } })
    {
        Turntable t (sineRecord (1000.0));
        t.settings.startSeconds = 0.05f;
        t.settings.transformRate = rate;
        t.settings.bpm = 120.0;
        t.key (deck::Key::play);
        t.play (0.5);
        t.key (deck::Key::transform);
        const auto from = t.play (1.0);
        INFO (deck::transformNames[rate]);
        CHECK (openings (t.out, from) == expected);
        t.key (deck::Key::transform, false);
        t.play (0.1);
        CHECK (t.lastRms (0.05) > 0.3);   // let go: open again
    }

    // With the song playing, the chop follows the song's beat, not when the key went down. 1/16 at 120 BPM: a cycle
    // is 0.25 beats (125 ms), cut for its second half. Pressed with the song at beat 0.0625, the first cut comes at
    // beat 0.125: 31.25 ms later. With the song stopped it runs from the press: the first cut 62.5 ms later.
    const auto firstCut = [] (double ppq)
    {
        Turntable t (sineRecord (1000.0));
        t.settings.startSeconds = 0.05f;
        t.settings.bpm = 120.0;
        t.key (deck::Key::play);
        t.play (0.5);
        t.settings.ppq = ppq;
        t.key (deck::Key::transform);
        const auto from = t.play (0.2);
        for (auto i = from; i + 24 <= t.out.size(); ++i)
            if (rms (t.out, i, i + 24) == 0.0)   // half a millisecond of silence: cut (the 1 ms ramp down done)
                return (double) (i - from) / sampleRate * 1000.0;
        return -1.0;
    };
    CHECK_THAT (firstCut (0.0625), WithinAbs (31.25 + 1.0, 0.1));   // (+ the 1 ms ramp down)
    CHECK_THAT (firstCut (-1.0), WithinAbs (62.5 + 1.0, 0.1));
}

TEST_CASE ("THE CROSSFADER in the plugin: FADER, CURVE and HAMSTER from the parameters; open by default", "[deck][fader][plugin]")
{
    Plugin p;
    CHECK (p.processor.getParameters().getRawParameterValue (ParamIDs::deckFader)->load() == 1.0f);
    p.set (ParamIDs::deckOn, 1.0f);
    p.set (ParamIDs::deckStart, 0.05f);
    p.set (ParamIDs::deckNeedle, 0.0f);
    const auto open = p.play (0.6, noteOn (24 + (int) deck::Key::drop4));
    const auto level = rms (open, (size_t) (0.3 * sampleRate), open.size());
    CHECK (level > 0.1);
    p.set (ParamIDs::deckFader, 0.0f);
    const auto cut = p.play (0.3);
    CHECK (rms (cut, (size_t) (0.01 * sampleRate), cut.size()) == 0.0);
    p.set (ParamIDs::deckHamster, 1.0f);   // turned round: the fader at 0 is open
    const auto turned = p.play (0.3);
    CHECK_THAT (rms (turned, (size_t) (0.01 * sampleRate), turned.size()), WithinAbs (level, level * 0.02));
}

//==============================================================================
// THE RACK (0.4.0, PR7-I's).
TEST_CASE ("THE DECK's rack: every control on it, each page showing its own, laid out inside the unit without overlapping", "[deck][ui][rack]")
{
    Plugin p;
    DeckPage page (p.processor);
    page.setSize (1476, 672);
    auto& rack = page.getRack();
    REQUIRE (rack.isVisible());
    CHECK (rack.getHeight() == DeckRack::preferredHeight);

    // Every deck parameter has its control on the rack, in rack style (PR7-I's big knob, value on a screen).
    const auto controls = page.getShownControls();
    juce::StringArray ids;
    for (auto* c : controls)
    {
        CHECK (c->getParentComponent() == &rack);
        CHECK (c->isRack());
        ids.add (c->getParameterId());
    }
    for (const auto* id : { ParamIDs::deckPitch, ParamIDs::deckSpeed, ParamIDs::deckStart, ParamIDs::deckBrake, ParamIDs::deckSpin,
                            ParamIDs::deckSpinTime, ParamIDs::deckReach, ParamIDs::deckNeedle, ParamIDs::deckFader, ParamIDs::deckCurve,
                            ParamIDs::deckTransform, ParamIDs::deckHamster, ParamIDs::deckKeys, ParamIDs::deckLevel,
                            ParamIDs::deckCues[0], ParamIDs::deckCues[1], ParamIDs::deckCues[2], ParamIDs::deckCues[3],
                            ParamIDs::deckScratchKeys, ParamIDs::deckScratchReach, ParamIDs::deckScratchRate, ParamIDs::deckModHand })
    {
        INFO (id);
        CHECK (ids.contains (id));
    }
    CHECK (rack.findChildWithID ("deckOn") != nullptr);
    CHECK (rack.findChildWithID ("deckLoad") != nullptr);
    CHECK (rack.findChildWithID ("deckHouse") != nullptr);

    // Each page: its controls shown, inside the unit, none on top of another.
    const auto face = rack.getLocalBounds().reduced (20);
    for (const auto pageKey : { DeckRack::Page::deck, DeckRack::Page::fader, DeckRack::Page::cues, DeckRack::Page::scratch })
    {
        rack.getPageKey (pageKey).onClick();
        CHECK (rack.getPage() == pageKey);
        CHECK (rack.getPageKey (pageKey).getToggleState());
        std::vector<juce::Rectangle<int>> shown;
        juce::StringArray shownIds;
        for (auto* c : controls)
            if (c->isVisible())
            {
                CHECK (face.contains (c->getBounds()));
                for (const auto& other : shown)
                    CHECK_FALSE (other.intersects (c->getBounds()));
                shown.push_back (c->getBounds());
                shownIds.add (c->getParameterId());
            }
        INFO ("page " << (int) pageKey << ": " << shownIds.joinIntoString (", "));
        CHECK (shownIds.contains (ParamIDs::deckLevel));   // LEVEL on every page (DECK OUT)
        CHECK (shownIds.contains (pageKey == DeckRack::Page::deck ? ParamIDs::deckPitch : pageKey == DeckRack::Page::fader ? ParamIDs::deckFader
                                  : pageKey == DeckRack::Page::cues ? ParamIDs::deckCues[0] : ParamIDs::deckScratchReach));
        CHECK (shownIds.size() == (pageKey == DeckRack::Page::deck ? 9 : pageKey == DeckRack::Page::fader ? 5 : pageKey == DeckRack::Page::cues ? 6 : 5));
    }

    // ON is the deck's switch.
    auto* on = dynamic_cast<juce::Button*> (rack.findChildWithID ("deckOn"));
    REQUIRE (on != nullptr);
    CHECK_FALSE (on->getToggleState());
    on->setToggleState (true, juce::sendNotificationSync);
    CHECK (p.processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() > 0.5f);

    // The screen names the record and how it's turning.
    page.refreshForTest();
    CHECK (rack.getScreen().record == "HOUSE RECORD");
    CHECK (rack.getScreen().state == "STOPPED");
}

TEST_CASE ("THE CROSSFADER on the page: dragging it moves FADER, and the deck is cut", "[deck][ui][fader]")
{
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    p.set (ParamIDs::deckStart, 0.05f);
    p.set (ParamIDs::deckNeedle, 0.0f);
    DeckPage page (p.processor);
    page.setSize (1476, 672);
    auto& fader = page.getCrossfader();
    CHECK (fader.getWidth() > 300);
    CHECK (fader.getValue() == 1.0);
    p.play (0.5, noteOn (24 + (int) deck::Key::drop4));
    fader.setValue (0.0, juce::sendNotificationSync);   // as a drag to the CUT end does
    CHECK (p.processor.getParameters().getRawParameterValue (ParamIDs::deckFader)->load() == 0.0f);
    const auto cut = p.play (0.3);
    CHECK (rms (cut, (size_t) (0.01 * sampleRate), cut.size()) == 0.0);
    page.refreshForTest();
    CHECK (p.processor.getDeck().getFaderGain() == 0.0f);
}

//==============================================================================
// THE SCRATCH LIBRARY (0.5.0, stage 4).
namespace
{
    // Fader openings (cut to open) in one run through a technique, from its shape.
    int shapeOpenings (int technique)
    {
        int count = 0;
        bool open = deck::scratchAt (technique, 0.0).open;
        for (int k = 1; k < 20000; ++k)   // (not phase 1: that's the next time round's start)
        {
            const auto now = deck::scratchAt (technique, k / 20000.0).open;
            count += now && ! open ? 1 : 0;
            open = now;
        }
        return count;
    }

    void scratchKey (Turntable& t, int technique, float velocity, bool down = true)
    {
        const auto note = t.settings.firstScratchKey + technique;
        t.pending.addEvent (down ? juce::MidiMessage::noteOn (1, note, velocity) : juce::MidiMessage::noteOff (1, note), 0);
    }
}

TEST_CASE ("THE SCRATCH LIBRARY's shapes: each starts and ends where the hand took the record, its cuts as described", "[deck][scratch]")
{
    REQUIRE (deck::scratchNames.size() == deck::numScratches);
    for (int t = 0; t < deck::numScratches; ++t)
    {
        INFO (deck::scratchNames[t]);
        CHECK (deck::scratchAt (t, 0.0).hand == 0.0);
        CHECK_THAT (deck::scratchAt (t, 1.0).hand, WithinAbs (0.0, 1.0e-9));
        for (int k = 0; k <= 1000; ++k)
        {
            const auto h = deck::scratchAt (t, k / 1000.0).hand;
            CHECK (h >= -1.0e-9);
            CHECK (h <= 1.0 + 1.0e-9);
        }
        CHECK (deck::scratchBeats (t) > 0.0);
    }
    // The fader's cuts: none on a BABY; FORWARD and BACKWARD heard on one half each; a FLARE's one click in each
    // stroke (two strokes: two openings after a click), a 2-CLICK FLARE's two, a CRAB's four taps a stroke, a
    // TRANSFORMER's sixteen chops.
    CHECK (shapeOpenings (0) == 0);
    CHECK (deck::scratchAt (1, 0.25).open);
    CHECK_FALSE (deck::scratchAt (1, 0.75).open);
    CHECK_FALSE (deck::scratchAt (2, 0.25).open);
    CHECK (deck::scratchAt (2, 0.75).open);
    CHECK (shapeOpenings (9) == 2);
    CHECK (shapeOpenings (10) == 4);
    CHECK (shapeOpenings (11) == 2 * 4 - 1);   // (it starts on a tap: the first isn't an opening)
    CHECK (shapeOpenings (8) == 16 - 1);       // (likewise its first chop)
    // TEAR stops half way back; SCRIBBLE's strokes are small.
    CHECK_THAT (deck::scratchAt (6, 0.57).hand, WithinAbs (0.5, 1.0e-9));
    for (int k = 0; k <= 100; ++k)
        CHECK (deck::scratchAt (7, k / 100.0).hand <= 0.12 + 1.0e-9);
}

TEST_CASE ("THE SCRATCH LIBRARY in the deck: BABY pushes the record a stroke forward and back in half a beat of the song", "[deck][scratch]")
{
    for (const auto bpm : { 120.0, 60.0 })
    {
        Turntable t (sineRecord (1000.0));
        t.settings.bpm = bpm;
        t.settings.scratchReach = 0.25f;   // a quarter turn: 0.45 s of the record at full velocity
        t.play (0.5);                      // (still: the motor off, the needle at the start)
        t.settings.cues[0] = 0.3f;
        t.key (deck::Key::drop1);          // somewhere in the middle (the motor comes on; HOLD keeps it still)
        t.key (deck::Key::hold);
        t.play (0.3);
        const auto start = t.deck.getPositionSeconds();
        scratchKey (t, 0, 1.0f);
        scratchKey (t, 0, 1.0f, false);    // let go at once: it plays its one stroke through
        const auto cycle = 0.5 * 60.0 / bpm;
        double furthest = 0.0;
        float fastestForward = 0.0f, fastestBack = 0.0f;
        for (int b = 0; b < (int) (cycle * sampleRate / blockSize); ++b)
        {
            t.play (blockSize / sampleRate);
            furthest = juce::jmax (furthest, t.deck.getPositionSeconds() - start);
            fastestForward = juce::jmax (fastestForward, t.deck.getSpeed());
            fastestBack = juce::jmin (fastestBack, t.deck.getSpeed());
            CHECK (t.deck.getScratch() == 0);
        }
        INFO (bpm << " BPM");
        CHECK_THAT (furthest, WithinAbs (0.25 * deck::secondsPerTurn, 0.02));
        CHECK (fastestForward > 1.0f);   // pushed faster than it plays...
        CHECK (fastestBack < -1.0f);     // ...and pulled back
        t.play (0.05);
        CHECK (t.deck.getScratch() == -1);   // done: the hand has let go
        CHECK_THAT (t.deck.getPositionSeconds(), WithinAbs (start, 0.005));   // back where it took the record
    }
}

TEST_CASE ("THE SCRATCH LIBRARY: velocity sets the stroke, a held key repeats, FORWARD is heard only on the push", "[deck][scratch]")
{
    const auto furthestFor = [] (float velocity)
    {
        Turntable t (sineRecord (1000.0));
        t.settings.cues[0] = 0.3f;
        t.key (deck::Key::drop1);
        t.key (deck::Key::hold);
        t.play (0.3);
        const auto start = t.deck.getPositionSeconds();
        scratchKey (t, 0, velocity);
        scratchKey (t, 0, velocity, false);
        double furthest = 0.0;
        for (int b = 0; b < 60; ++b)
        {
            t.play (blockSize / sampleRate);
            furthest = juce::jmax (furthest, t.deck.getPositionSeconds() - start);
        }
        return furthest;
    };
    CHECK_THAT (furthestFor (1.0f / 127.0f) / furthestFor (1.0f), WithinAbs (0.4 + 0.6 / 127.0, 0.03));

    // Held: it keeps going round; let go, it finishes the stroke it's on.
    Turntable t (sineRecord (1000.0));
    t.settings.cues[0] = 0.3f;
    t.key (deck::Key::drop1);
    t.key (deck::Key::hold);
    t.play (0.3);
    scratchKey (t, 0, 1.0f);
    t.play (0.9);   // over three strokes at 120 BPM (0.25 s each)
    CHECK (t.deck.getScratch() == 0);
    scratchKey (t, 0, 1.0f, false);
    t.play (0.3);
    CHECK (t.deck.getScratch() == -1);

    // FORWARD: the push heard, the pull cut (the fader shut: silence).
    Turntable f (sineRecord (1000.0));
    f.settings.cues[0] = 0.3f;
    f.key (deck::Key::drop1);
    f.key (deck::Key::hold);
    f.play (0.3);
    scratchKey (f, 1, 1.0f);
    scratchKey (f, 1, 1.0f, false);
    const auto from = f.play (0.25);
    const auto quarter = (size_t) (0.0625 * sampleRate);
    CHECK (rms (f.out, from + quarter / 2, from + 3 * quarter / 2) > 0.1);   // the push, at speed
    CHECK (rms (f.out, from + 5 * quarter / 2 + 96, from + 7 * quarter / 2) == 0.0);   // the pull, cut
}

TEST_CASE ("MOD HAND: the mod wheel moves the record and stays; off, the mod wheel leaves the record alone", "[deck][scratch]")
{
    for (const auto on : { true, false })
    {
        Turntable t (sineRecord (1000.0));
        t.settings.modHand = on;
        t.settings.reachTurns = 0.25f;
        t.settings.cues[0] = 0.3f;
        t.key (deck::Key::drop1);
        t.key (deck::Key::hold);
        t.play (0.3);
        t.key (deck::Key::hold, false);
        t.key (deck::Key::stop);
        t.settings.brakeSeconds = 0.05f;
        t.play (0.3);   // stopped
        const auto start = t.deck.getPositionSeconds();
        t.pending.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);   // the wheel all the way up
        t.play (0.1);
        INFO ("MOD HAND " << (on ? "on" : "off"));
        // On: the whole wheel is twice REACH, half a turn: 0.9 s of the record forward. Off: it doesn't move.
        CHECK_THAT (t.deck.getPositionSeconds() - start, WithinAbs (on ? 0.5 * deck::secondsPerTurn : 0.0, 0.01));
        t.play (0.3);   // left there: the hand lets go, the record stays (the motor's off)
        CHECK_THAT (t.deck.getPositionSeconds() - start, WithinAbs (on ? 0.5 * deck::secondsPerTurn : 0.0, 0.01));
    }
}

TEST_CASE ("THE SCRATCH LIBRARY in the plugin: its keys (C2 by default) play techniques, not the synth; OFF gives them back", "[deck][scratch][plugin]")
{
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    p.set (ParamIDs::deckLevel, -60.0f);   // the deck silent: what's heard would be the synth
    const auto played = p.play (0.1, noteOn (36));
    CHECK (p.processor.getDeck().getScratch() == 0);   // BABY (C2) playing
    CHECK (rms (played, 0, played.size()) == 0.0);     // no synth note
    p.play (0.5);

    // OFF with BABY still held: its note-off would go to the synth now, so the deck lets go (it finishes its stroke;
    // it doesn't go round for ever).
    p.set (ParamIDs::deckScratchKeys, 0.0f);   // OFF
    const auto synth = p.play (0.3, noteOn (37));
    CHECK (p.processor.getDeck().getScratch() == -1);
    CHECK (rms (synth, 0, synth.size()) > 1.0e-3);
}

TEST_CASE ("THE SCRATCH page: its techniques listed with their keys; clicking one plays it while held", "[deck][ui][scratch]")
{
    Plugin p;
    p.set (ParamIDs::deckOn, 1.0f);
    DeckPage page (p.processor);
    page.setSize (1476, 672);
    auto& rack = page.getRack();
    rack.getPageKey (DeckRack::Page::scratch).onClick();
    page.refreshForTest();
    // BABY's line, at the middle of its row: the first technique.
    int found = -1;
    for (int y = 0; y < rack.getHeight() && found < 0; y += 2)
        for (int x = rack.getWidth() / 2; x < rack.getWidth() && found < 0; x += 2)
            if (rack.scratchAt ({ x, y }) == 0)
                found = y * 10000 + x;
    REQUIRE (found >= 0);
    const juce::Point<int> at (found % 10000 + 10, found / 10000 + 3);
    REQUIRE (rack.scratchAt (at) == 0);
    REQUIRE (rack.onScratchPressed != nullptr);
    rack.onScratchPressed (rack.scratchAt (at), true);
    p.play (0.1);
    CHECK (p.processor.getDeck().getScratch() == 0);
    page.refreshForTest();
    rack.onScratchPressed (0, false);
    p.play (0.4);
    CHECK (p.processor.getDeck().getScratch() == -1);
    // The page's own: other pages have no techniques to click.
    rack.getPageKey (DeckRack::Page::deck).onClick();
    CHECK (rack.scratchAt (at) == -1);
}

TEST_CASE ("THE DECK lets go of held keys when its keys move (their note-offs would go to the synth): no stuck HOLD", "[deck][scratch]")
{
    Turntable t (sineRecord (1000.0));
    t.settings.startSeconds = 0.05f;
    t.key (deck::Key::play);
    t.play (0.3);
    t.key (deck::Key::hold);
    t.play (0.1);
    CHECK (std::abs (t.deck.getSpeed()) < 0.01f);   // held
    t.settings.firstKey = 36;                        // KEYS moved, HOLD's note-off never coming
    t.play (0.2);
    CHECK_THAT (t.deck.getSpeed(), WithinAbs (1.0, 0.01));   // let go: back with the platter
}
