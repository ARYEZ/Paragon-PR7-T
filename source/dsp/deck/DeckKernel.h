#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <vector>

// The deck's two filters, shared by records (Deck.cpp) and THE LIVE RECORD's tape (LiveTape.cpp): the interpolator's
// windowed-sinc kernel and the half-band filter its slower copies are made with.
namespace deck::kernels
{
    // THE INTERPOLATOR's kernel: a sinc, windowed (Kaiser, beta 8: about 80 dB down outside the band) to 8 zero
    // crossings either side, tabled at 512 points per crossing and read by straight lines between them.
    constexpr int zeroCrossings = 8;
    constexpr int tableResolution = 512;
    constexpr double kaiserBeta = 8.0;

    inline double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
            if (term < 1.0e-12 * sum)
                break;
        }
        return sum;
    }

    inline const std::vector<float>& kernelTable()
    {
        static const std::vector<float> table = []
        {
            std::vector<float> t ((size_t) (zeroCrossings * tableResolution + 2), 0.0f);
            const auto norm = besselI0 (kaiserBeta);
            for (int k = 0; k <= zeroCrossings * tableResolution; ++k)
            {
                const auto u = (double) k / tableResolution;
                const auto sinc = k == 0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * u) / (juce::MathConstants<double>::pi * u);
                const auto x = u / zeroCrossings;
                t[(size_t) k] = (float) (sinc * besselI0 (kaiserBeta * std::sqrt (juce::jmax (0.0, 1.0 - x * x))) / norm);
            }
            return t;
        }();
        return table;
    }

    // The kernel at `u` zero crossings from its centre (0 beyond the window).
    inline float kernel (const float* table, double u) noexcept
    {
        const auto at = std::abs (u) * tableResolution;
        const auto index = (int) at;
        if (index >= zeroCrossings * tableResolution)
            return 0.0f;
        const auto frac = (float) (at - index);
        return table[index] + frac * (table[index + 1] - table[index]);
    }

    // The half-band filter: a windowed sinc, 127 taps (Kaiser beta 9, about 90 dB down), its cutoff at 0.454 of the
    // band so its stopband starts at the new Nyquist (nothing folds back as it's halved); flat to about 0.41 of the
    // band. Centred (it looks ahead as far as behind), so every copy lines up with the sample.
    inline const std::vector<double>& halfBand()
    {
        static const std::vector<double> h = []
        {
            constexpr int side = 63;
            constexpr double cutoff = 0.5 * 0.454, beta = 9.0;
            std::vector<double> taps ((size_t) (2 * side + 1));
            double sum = 0.0;
            for (int n = -side; n <= side; ++n)
            {
                const auto x = 2.0 * cutoff * n;
                const auto sinc = n == 0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
                const auto w = besselI0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - std::pow ((double) n / side, 2.0)))) / besselI0 (beta);
                taps[(size_t) (n + side)] = sinc * w;
                sum += sinc * w;
            }
            for (auto& t : taps)
                t /= sum;   // exactly unity at DC
            return taps;
        }();
        return h;
    }

    // Which copy (level) to read at `pace` (record samples per output sample), and the kernel's cutoff in that copy:
    // a copy has 1/2^level of the band; between copies the kernel widens (its cutoff lowered) for what's left, so the
    // brightness falls smoothly as the speed rises; never so low the kernel would reach past `padding` samples.
    struct Reading
    {
        int level = 0;
        double scale = 1.0, cutoff = 1.0, reach = zeroCrossings;
    };
    inline Reading readingFor (double pace, int numLevels, int padding) noexcept
    {
        Reading r;
        r.level = pace > 1.0 ? juce::jmin (numLevels - 1, (int) std::floor (std::log2 (pace))) : 0;
        r.scale = (double) (1 << r.level);
        r.cutoff = juce::jlimit ((double) zeroCrossings / (padding - 2), 1.0, r.scale / juce::jmax (pace, 1.0e-9));
        r.reach = zeroCrossings / r.cutoff;
        return r;
    }

    // The kernel summed over x[0 .. count - 1], which are the copy's samples `first` to `first + count - 1`, read at
    // `p` (in the copy's samples).
    inline float sum (const float* x, std::int64_t first, int count, double p, double cutoff) noexcept
    {
        const auto* table = kernelTable().data();
        double total = 0.0;
        const auto from = p - (double) first;
        for (int j = 0; j < count; ++j)
            total += (double) kernel (table, (from - j) * cutoff) * x[j];
        return (float) (total * cutoff);
    }
}
