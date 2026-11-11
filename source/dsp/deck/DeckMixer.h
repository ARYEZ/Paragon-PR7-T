#pragma once

#include <juce_core/juce_core.h>

#include <array>

// THE DECK's sound (PR7-T 0.6.0, stage 6): what's between the needle and the speakers on a DJ's setup. Our own
// designs:
//  - CART: the cartridge's voicing. CLEAN (flat), SCRATCH (the presence lift and softer top scratch cartridges are
//    known for), WARM (rounder highs, a little low weight). Voicings, not models of any maker's cartridge.
//  - The mixer channel's 3-band EQ as an isolator, as on battle mixers: the sound split into LOW, MID and HIGH by
//    Linkwitz-Riley crossovers (250 Hz, 2.5 kHz; the low band through an allpass at 2.5 kHz so the three add back up
//    flat: within 0.0002 dB at 0 dB), each band turned up to +6 dB, down, or KILLed.
//  - The one-knob DJ FILTER: left of centre a low-pass sweeping from 20 kHz down to 60 Hz, right of centre a
//    high-pass from 20 Hz up to 8 kHz, RES its resonance (a state-variable filter).
// Each section is bypassed exactly while it's neutral (CLEAN, every band at 0 dB with no kills, the filter centred),
// so the deck sounds as it did before; it crossfades in and out over 10 ms, so switching never clicks.
namespace deck
{
    inline const juce::StringArray cartNames { "CLEAN", "SCRATCH", "WARM" };   // by index: only append

    struct MixerSettings
    {
        int cart = 0;                                  // cartNames
        std::array<float, 3> eqDb { 0.0f, 0.0f, 0.0f }; // LOW, MID, HIGH: -40 (off) to +6 dB
        std::array<bool, 3> kill { false, false, false };
        float filter = 0.0f;                           // -1 (low-pass, 60 Hz) to +1 (high-pass, 8 kHz); 0 off
        float resonance = 0.3f;                        // 0 to 1
        static constexpr float eqFloorDb = -40.0f;     // a band at the floor is off (as a kill)
    };

    class DeckMixer
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void set (const MixerSettings&) noexcept;            // once a block
        void process (float& left, float& right) noexcept;   // a sample

        static constexpr double crossLow = 250.0, crossHigh = 2500.0;
        static double filterHz (float filter) noexcept;      // the FILTER's cutoff at a setting (0: none)

    private:
        // A biquad (transposed direct form II, in double), its coefficients set off the sample loop.
        struct Biquad
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
            double run (double x) noexcept
            {
                const auto y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void clear() noexcept { z1 = z2 = 0.0; }
        };
        enum class Kind { lowPass, highPass, allPass, peak, lowShelf, highShelf };
        static void design (Biquad&, Kind, double hz, double q, double gainDb, double rate) noexcept;

        double rate = 48000.0;
        MixerSettings settings;

        // CART: a low shelf, a peak, a high shelf a side.
        std::array<std::array<Biquad, 3>, 2> cart;
        int cartDesigned = -1;
        // The isolator: LR4 (two Butterworth stages) low and high at each crossover, the low band's allpass.
        struct Isolator { Biquad lowA, lowB, highA, highB, allpass, midA, midB, topA, topB; };
        std::array<Isolator, 2> isolator;
        std::array<double, 3> bandGain { 1.0, 1.0, 1.0 }, bandTarget { 1.0, 1.0, 1.0 };
        // The FILTER: a TPT state-variable filter a side, its setting smoothed, its coefficients every 16 samples.
        struct Svf { double s1 = 0.0, s2 = 0.0; };
        std::array<Svf, 2> svf;
        double filterNow = 0.0, filterG = 0.0, filterK = 1.414;
        int filterCountdown = 0;

        // Each section's share of the sound (0: bypassed exactly), moving 10 ms to where it's wanted.
        double cartMix = 0.0, eqMix = 0.0, filterMix = 0.0;
        double mixStep = 0.0, gainCoeff = 0.0, filterCoeff = 0.0;
    };
}
