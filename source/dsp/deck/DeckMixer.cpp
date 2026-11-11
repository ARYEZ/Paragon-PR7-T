#include "DeckMixer.h"

#include <cmath>

namespace deck
{
    void DeckMixer::prepare (double sampleRate)
    {
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        mixStep = 1.0 / (0.01 * rate);                          // a section in or out: 10 ms
        gainCoeff = 1.0 - std::exp (-1.0 / (0.005 * rate));     // a band's gain: 5 ms (a kill is a quick, clean cut)
        filterCoeff = 1.0 - std::exp (-1.0 / (0.02 * rate));    // the FILTER's knob: 20 ms
        for (int c = 0; c < 2; ++c)
        {
            auto& iso = isolator[(size_t) c];
            design (iso.lowA, Kind::lowPass, crossLow, 0.7071, 0.0, rate);
            design (iso.lowB, Kind::lowPass, crossLow, 0.7071, 0.0, rate);
            design (iso.highA, Kind::highPass, crossLow, 0.7071, 0.0, rate);
            design (iso.highB, Kind::highPass, crossLow, 0.7071, 0.0, rate);
            design (iso.allpass, Kind::allPass, crossHigh, 0.7071, 0.0, rate);
            design (iso.midA, Kind::lowPass, crossHigh, 0.7071, 0.0, rate);
            design (iso.midB, Kind::lowPass, crossHigh, 0.7071, 0.0, rate);
            design (iso.topA, Kind::highPass, crossHigh, 0.7071, 0.0, rate);
            design (iso.topB, Kind::highPass, crossHigh, 0.7071, 0.0, rate);
        }
        cartDesigned = -1;
        reset();
    }

    void DeckMixer::reset()
    {
        for (auto& side : cart)
            for (auto& b : side)
                b.clear();
        for (auto& iso : isolator)
            for (auto* b : { &iso.lowA, &iso.lowB, &iso.highA, &iso.highB, &iso.allpass, &iso.midA, &iso.midB, &iso.topA, &iso.topB })
                b->clear();
        for (auto& s : svf)
            s = {};
        cartMix = eqMix = filterMix = 0.0;
        filterNow = 0.0;
        bandGain = bandTarget = { 1.0, 1.0, 1.0 };
    }

    double DeckMixer::filterHz (float filter) noexcept
    {
        if (filter < 0.0f)
            return 20000.0 * std::pow (60.0 / 20000.0, (double) -filter);   // low-pass: 20 kHz to 60 Hz
        if (filter > 0.0f)
            return 20.0 * std::pow (8000.0 / 20.0, (double) filter);        // high-pass: 20 Hz to 8 kHz
        return 0.0;
    }

    void DeckMixer::design (Biquad& b, Kind kind, double hz, double q, double gainDb, double sampleRate) noexcept
    {
        // RBJ's cookbook.
        const auto w = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.49, hz) / sampleRate;
        const auto cw = std::cos (w), sw = std::sin (w);
        const auto alpha = sw / (2.0 * q);
        const auto A = std::pow (10.0, gainDb / 40.0);
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;
        switch (kind)
        {
            case Kind::lowPass:  b0 = (1.0 - cw) / 2.0; b1 = 1.0 - cw; b2 = b0; a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
            case Kind::highPass: b0 = (1.0 + cw) / 2.0; b1 = -(1.0 + cw); b2 = b0; a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
            case Kind::allPass:  b0 = 1.0 - alpha; b1 = -2.0 * cw; b2 = 1.0 + alpha; a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha; break;
            case Kind::peak:
                b0 = 1.0 + alpha * A; b1 = -2.0 * cw; b2 = 1.0 - alpha * A; a0 = 1.0 + alpha / A; a1 = -2.0 * cw; a2 = 1.0 - alpha / A; break;
            case Kind::lowShelf:
            {
                const auto s = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1.0) - (A - 1.0) * cw + s); b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw); b2 = A * ((A + 1.0) - (A - 1.0) * cw - s);
                a0 = (A + 1.0) + (A - 1.0) * cw + s; a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw); a2 = (A + 1.0) + (A - 1.0) * cw - s;
                break;
            }
            case Kind::highShelf:
            {
                const auto s = 2.0 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1.0) + (A - 1.0) * cw + s); b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw); b2 = A * ((A + 1.0) + (A - 1.0) * cw - s);
                a0 = (A + 1.0) - (A - 1.0) * cw + s; a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw); a2 = (A + 1.0) - (A - 1.0) * cw - s;
                break;
            }
        }
        b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0; b.a1 = a1 / a0; b.a2 = a2 / a0;
    }

    void DeckMixer::set (const MixerSettings& s) noexcept
    {
        settings = s;
        // CART: our voicings. SCRATCH: +3 dB of presence at 3.2 kHz, the top softened (-4 dB over 12 kHz), a touch
        // less low end. WARM: the highs rounded (-3.5 dB over 6 kHz), +2 dB of low weight at 120 Hz.
        if (s.cart != cartDesigned && s.cart > 0)
        {
            struct Voice { double lowHz, lowDb, peakHz, peakDb, peakQ, highHz, highDb; };
            const Voice voices[] = { {}, { 80.0, -1.5, 3200.0, 3.0, 0.9, 12000.0, -4.0 }, { 120.0, 2.0, 1000.0, 0.0, 0.7, 6000.0, -3.5 } };
            const auto& v = voices[juce::jlimit (1, 2, s.cart)];
            for (auto& side : cart)
            {
                design (side[0], Kind::lowShelf, v.lowHz, 0.7071, v.lowDb, rate);
                design (side[1], Kind::peak, v.peakHz, v.peakQ, v.peakDb, rate);
                design (side[2], Kind::highShelf, v.highHz, 0.7071, v.highDb, rate);
            }
            cartDesigned = s.cart;
        }
        for (size_t band = 0; band < 3; ++band)
            bandTarget[band] = (s.kill[band] || s.eqDb[band] <= MixerSettings::eqFloorDb + 0.5f) ? 0.0 : std::pow (10.0, s.eqDb[band] / 20.0);
        filterK = 1.414 - 1.29 * juce::jlimit (0.0, 1.0, (double) s.resonance);   // 1 / Q: 0.707 to about 8
    }

    void DeckMixer::process (float& left, float& right) noexcept
    {
        const auto towards = [this] (double& mix, bool wanted) { mix = wanted ? juce::jmin (1.0, mix + mixStep) : juce::jmax (0.0, mix - mixStep); };
        const auto cartWanted = settings.cart > 0;
        const auto eqWanted = ! juce::exactlyEqual (bandTarget[0], 1.0) || ! juce::exactlyEqual (bandTarget[1], 1.0) || ! juce::exactlyEqual (bandTarget[2], 1.0);
        // The FILTER turned across the centre (low-pass to high-pass or back): it fades out, changes over silent in
        // the mix, and fades in on its new side (switching type while heard would click).
        const auto sameSide = (settings.filter < 0.0f) == (filterNow < 0.0);
        const auto filterWanted = std::abs (settings.filter) > 0.01f && (sameSide || filterMix <= 0.0);
        if (! cartWanted && ! eqWanted && ! filterWanted && cartMix <= 0.0 && eqMix <= 0.0 && filterMix <= 0.0)
            return;   // neutral: the sound exactly as it came

        double x[2] = { left, right };

        // CART.
        if (cartMix <= 0.0 && cartWanted)
            for (auto& side : cart)
                for (auto& b : side)
                    b.clear();
        towards (cartMix, cartWanted);
        if (cartMix > 0.0 && cartDesigned > 0)
            for (int c = 0; c < 2; ++c)
            {
                auto& side = cart[(size_t) c];
                const auto wet = side[2].run (side[1].run (side[0].run (x[c])));
                x[c] += cartMix * (wet - x[c]);
            }

        // The isolator.
        if (eqMix <= 0.0 && eqWanted)
            for (auto& iso : isolator)
                for (auto* b : { &iso.lowA, &iso.lowB, &iso.highA, &iso.highB, &iso.allpass, &iso.midA, &iso.midB, &iso.topA, &iso.topB })
                    b->clear();
        towards (eqMix, eqWanted);
        for (size_t band = 0; band < 3; ++band)
            bandGain[band] += gainCoeff * (bandTarget[band] - bandGain[band]);
        if (eqMix > 0.0)
            for (int c = 0; c < 2; ++c)
            {
                auto& iso = isolator[(size_t) c];
                const auto low = iso.allpass.run (iso.lowB.run (iso.lowA.run (x[c])));
                const auto rest = iso.highB.run (iso.highA.run (x[c]));
                const auto mid = iso.midB.run (iso.midA.run (rest));
                const auto top = iso.topB.run (iso.topA.run (rest));
                const auto wet = low * bandGain[0] + mid * bandGain[1] + top * bandGain[2];
                x[c] += eqMix * (wet - x[c]);
            }

        // The FILTER.
        if (filterMix <= 0.0 && filterWanted)
        {
            for (auto& s : svf)
                s = {};
            filterNow = settings.filter;
            filterCountdown = 0;
        }
        towards (filterMix, filterWanted);
        if (filterMix > 0.0)
        {
            if (sameSide)   // (held where it is while it fades out to cross over)
                filterNow += filterCoeff * ((double) settings.filter - filterNow);
            if (--filterCountdown <= 0)
            {
                filterCountdown = 16;
                const auto hz = filterHz ((float) (std::abs (filterNow) < 1.0e-4 ? (filterNow < 0.0 ? -1.0e-4 : 1.0e-4) : filterNow));
                filterG = std::tan (juce::MathConstants<double>::pi * juce::jlimit (10.0, rate * 0.45, hz) / rate);
            }
            const auto g = filterG, k = filterK;
            const auto a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
            for (int c = 0; c < 2; ++c)
            {
                auto& s = svf[(size_t) c];
                const auto v3 = x[c] - s.s2;
                const auto v1 = a1 * s.s1 + a2 * v3;
                const auto v2 = s.s2 + a2 * s.s1 + a3 * v3;
                s.s1 = 2.0 * v1 - s.s1;
                s.s2 = 2.0 * v2 - s.s2;
                const auto lowPass = v2, highPass = x[c] - k * v1 - v2;
                const auto wet = filterNow < 0.0 ? lowPass : highPass;
                x[c] += filterMix * (wet - x[c]);
            }
        }
        left = (float) x[0];
        right = (float) x[1];
    }
}
