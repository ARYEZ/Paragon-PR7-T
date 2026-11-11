#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

// THE DECK (PR7-T 0.2.0): a turntable that plays a record (a sample) at whatever speed and direction the
// platter's motor and the DJ's hand give it, as nu-metal's DJs played theirs: scratched, held, spun back, stopped,
// the needle dropped. Our own design and code.
//
// How it sounds like a turntable, not a sample player:
//  - The record's place is what moves. The motor turns the platter, the slipmat lets the record slip on it, and a hand
//    on the record moves it (forwards, backwards, held still). The speed and direction are what those give, from
//    moment to moment; the sound is read from wherever the needle is.
//  - Loudness follows speed, as a real cartridge's does (its voltage comes from how fast the groove moves under the
//    needle): a slow drag is quiet, a fast scratch loud, a record held still is silent (not stuck on a sample value).
//    At normal speed it's exactly the sample's level; capped at twice that (+6 dB).
//  - Clean at any speed: sped up, a sample's highs would go past what the host's rate can hold and fold back down as
//    harsh, grainy aliasing. The record keeps copies filtered to a half, a quarter and an eighth of its band, and is
//    read through a windowed-sinc interpolator whose cutoff follows the speed, so a fast scratch is bright but clean.
namespace deck
{
    // A record: the sample, and the filtered copies the deck reads it from at speed.
    struct Record
    {
        static constexpr int numLevels = 4;       // the sample, then filtered to 1/2, 1/4, 1/8 of its band (and rate)
        static constexpr int padding = 40;        // silence either side of every level, so the interpolator never checks bounds
        juce::String name;
        double sampleRate = 48000.0;
        int length = 0;                            // in the sample's own samples
        // Each level stereo, `padding` samples of silence either side: level k has (length >> k) + 1 samples between.
        std::array<juce::AudioBuffer<float>, numLevels> levels;
        // For the page's picture: the loudest sample in each of `overviewBins` equal slices, 0 to 1.
        static constexpr int overviewBins = 600;
        std::array<float, overviewBins> overview {};

        double seconds() const noexcept { return length / sampleRate; }
    };

    // Makes a record from audio (mono is played on both sides). The filtered copies are built here (a few ms a
    // second of audio): call it off the audio thread.
    std::shared_ptr<const Record> makeRecord (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name);

    // THE HOUSE RECORD: our own scratch record, built in, so there's something to play before a sample is loaded.
    // Four sounds two seconds apart, one at each cue's default (0%, 25%, 50%, 75% of its 8 s):
    //  - an "ahh", a sung vowel (a voice's pulses through three formants, falling a little in pitch);
    //  - a horn stab (a brass-like chord: saws, the filter opening and closing on it);
    //  - a short drum break (kick, snare, hats at 95 BPM);
    //  - a 1 kHz tone, as on a DJ's practice record (good for transforms, and for the tests to measure pitch by).
    // All synthesized here; no one's recording.
    std::shared_ptr<const Record> houseRecord();
    inline constexpr double houseToneStart = 6.0, houseToneSeconds = 1.6, houseToneHz = 1000.0;

    // One record's place in seconds, per turn of the platter: the groove's 33 1/3 RPM (60 / 33.333 s).
    inline constexpr double secondsPerTurn = 1.8;

    // What the 12 keys of the deck's block do, from its first (C).
    // 0.4.0: CUT (F#) and TRANSFORM (G#), the crossfader's keys. A# and B are free for the scratch library (stage 4).
    enum class Key { play = 0, stop = 1, backspin = 2, hold = 3, drop1 = 4, drop2 = 5, cut = 6, drop3 = 7, transform = 8, drop4 = 9 };
    inline constexpr int numKeys = 12;
    inline const juce::StringArray keyNames { "PLAY", "STOP", "BACKSPIN", "HOLD", "DROP 1", "DROP 2", "CUT", "DROP 3", "TRANSFORM", "DROP 4", "", "" };

    // THE CROSSFADER (0.4.0, stage 3): the deck's channel on a DJ mixer, cut in and out by the fader, half of every
    // scratch. TRANSFORM chops it in time with the song at one of these rates (beats a cycle: open half, cut half).
    inline const juce::StringArray transformNames { "1/8", "1/16", "1/32", "1/8T", "1/16T" };   // by index: only append
    inline constexpr double transformBeats[] = { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
    // How loud the deck is with the fader at `fader` (0 to 1 of its travel; 1: all the way to the deck's side) and
    // CURVE at `curve`: 0 a smooth mixing fade (a quarter sine over the whole travel, -3 dB at the middle), 1 a scratch
    // cut (fully open by 1% of the travel, as a scratch mixer's is); HAMSTER turns the fader round.
    float faderGain (float fader, float curve, bool hamster) noexcept;

    struct Settings
    {
        float pitch = 0.0f;              // the pitch fader: -0.16 to +0.16 (+-16%)
        bool speed45 = false;            // 45 RPM: 1.35 times as fast as 33 1/3
        float startSeconds = 0.7f;       // the motor from standstill to full speed
        float brakeSeconds = 1.2f;       // STOP: full speed to standstill (long: a tape stop)
        float spinSpeed = 3.0f;          // BACKSPIN: how fast the record is thrown back, times normal speed
        float spinSeconds = 1.0f;        // BACKSPIN: how long it takes to slow to a stop
        float reachTurns = 0.25f;        // the pitch wheel all the way: how far the hand moves the record, in turns
        float needle = 0.5f;             // the needle drop's thump and crackle, 0 to 1
        float levelDb = 0.0f;            // the deck's level
        std::array<float, 4> cues { 0.0f, 0.25f, 0.5f, 0.75f };   // DROP 1-4: where the needle lands, 0 to 1 of the record
        int firstKey = 24;               // the deck's 12 keys from this MIDI note (24: C1, the keyboard's lowest)
        // THE CROSSFADER (0.4.0).
        float fader = 1.0f;              // 0 to 1: 1 open (the deck heard), 0 cut
        float curve = 1.0f;              // 0: a smooth mixing fade, 1: a scratch cut
        bool hamster = false;            // the fader turned round
        int transformRate = 1;           // transformNames: 1/16
        double bpm = 120.0;              // the song's tempo, for TRANSFORM
        double ppq = -1.0;               // where the song is at the block's start, in beats (-1: stopped; TRANSFORM runs free)

        float baseSpeed() const noexcept { return (1.0f + pitch) * (speed45 ? 1.35f : 1.0f); }
    };

    class Deck
    {
    public:
        Deck();

        void prepare (double hostSampleRate);
        void reset();   // the record back at the start, the motor off, no hand on it

        // Adds `numSamples` of the deck's sound into left and right (right may be null: mono), from `record`
        // (null: silence, the deck still turning). The MIDI's notes in the deck's 12 keys and its pitch wheel (the
        // hand) take effect on their samples: events at `firstSample` to `firstSample + numSamples` of `midi`.
        void process (float* left, float* right, int numSamples, const juce::MidiBuffer& midi, int firstSample,
                      const Record* record, const Settings&) noexcept;

        // The mouse's hand on the platter (the page): while `holding`, the record follows it, `turns` being how far
        // it has been turned since it was grabbed (forward positive). Read at the start of each block.
        void setMouseHand (bool holding, float turns) noexcept { mouseTurns = turns; mouseHolding = holding; }

        // What the page shows (any thread).
        double getPositionSeconds() const noexcept { return shownPosition.load (std::memory_order_relaxed); }
        float getSpeed() const noexcept { return shownSpeed.load (std::memory_order_relaxed); }   // times the sample's own speed
        bool isMotorOn() const noexcept { return shownMotor.load (std::memory_order_relaxed); }
        float getFaderGain() const noexcept { return shownFader.load (std::memory_order_relaxed); }   // the crossfader's, now
        // The deck's own loudest sample (after LEVEL and the crossfader) since this was last asked: the rack's meter.
        float takePeak() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }

        // For tests: the needle's random crackle the same each run.
        void seedRandom (std::int64_t seed) noexcept { random.setSeed (seed); }

        // Reads `record`'s side `channel` at `position` (in the sample's own samples) as it sounds moving at
        // `rate` samples of it per output sample (its magnitude: the interpolator's cutoff follows it).
        static float read (const Record&, int channel, double position, double rate) noexcept;

    private:
        enum class Hand { none, wheel, mouse };
        void handleMidi (const juce::MidiMessage&, const Record*, const Settings&) noexcept;
        void dropNeedle (double toSamples, float velocity) noexcept;
        void grab (Hand) noexcept;

        double hostRate = 48000.0;

        // Where the needle is (in the record's samples) and how fast the record turns (times its natural speed: the
        // sample's pitch at 1). The platter is the motor's: the slipmat lets the record follow it.
        double position = 0.0;
        double speed = 0.0, platter = 0.0;
        bool motorOn = false;

        // The hand on the record: the pitch wheel's or the mouse's, moving its place from where it grabbed it.
        Hand hand = Hand::none;
        double handAnchor = 0.0, handSmooth1 = 0.0, handSmooth2 = 0.0;
        float wheel = 0.0f;                    // -1 to 1 (the pitch wheel's place from its centre)
        int wheelCentredFor = 0;               // samples the wheel has sat at its centre (it lets go after 50 ms)
        bool holdKey = false;                  // HOLD: a hand holding the record still
        bool cutKey = false, transformKey = false;   // the crossfader's keys, held
        double transformBeat = 0.0;            // TRANSFORM running free (the song stopped): beats since it was pressed
        float faderNow = 1.0f;                 // the crossfader's gain, moving to where it's set in straight 1 ms lines
        std::atomic<bool> mouseHolding { false };   // set by the page (the message thread), read by the audio thread
        std::atomic<float> mouseTurns { 0.0f };

        // BACKSPIN: thrown back, then slowing to a stop over `spinSeconds`.
        bool spinning = false;
        double spinElapsed = 0.0;

        // A jump (a needle drop): the old place fading out under the new, 4 ms.
        double fadePosition = 0.0;
        int fadeLeft = 0, fadeLength = 1;

        // The needle landing: its thump (a low sine dying away) and a burst of crackle.
        double thumpPhase = 0.0;
        float thumpLevel = 0.0f, crackleLevel = 0.0f;
        juce::Random random { 7 };

        juce::SmoothedValue<float> gain;

        std::atomic<double> shownPosition { 0.0 };
        std::atomic<float> shownSpeed { 0.0f };
        std::atomic<bool> shownMotor { false };
        std::atomic<float> shownFader { 1.0f };
        std::atomic<float> peak { 0.0f };
    };
}
