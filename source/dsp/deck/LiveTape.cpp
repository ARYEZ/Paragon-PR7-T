#include "LiveTape.h"
#include "DeckKernel.h"

namespace deck
{
    void LiveTape::prepare (double sampleRate)
    {
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;
        const auto wanted = (int) std::ceil (seconds * rate) + 4096;   // the seconds kept, and the filters' reach
        auto size = 1;
        while (size < wanted)
            size <<= 1;
        for (int k = 0; k < numLevels; ++k)
        {
            sizes[(size_t) k] = size >> k;
            for (auto& side : rings[(size_t) k])
                side.assign ((size_t) sizes[(size_t) k], 0.0f);
        }
        binSamples = juce::jmax (1, (int) std::round (seconds * rate / bins));
        reset();
    }

    void LiveTape::reset() noexcept
    {
        for (auto& level : rings)
            for (auto& side : level)
                std::fill (side.begin(), side.end(), 0.0f);
        written.fill (0);
        start = 0;
        started = false;
        hitCount = hitWrite = 0;
        binFill = 0;
        binPeak = 0.0f;
        for (auto& b : shownBins)
            b.store (0.0f, std::memory_order_relaxed);
        shownWrite.store (bins - 1, std::memory_order_relaxed);
    }

    void LiveTape::write (const float* left, const float* right, int n, std::int64_t at) noexcept
    {
        if (n <= 0 || sizes[0] == 0)
            return;
        if (started && at != written[0])
        {
            // A gap or a jump back: what's kept no longer lines up with the clock, so the tape starts again from here
            // (not cleared: that's megabytes on the audio thread; nothing from before `start` is read).
            started = false;
            hitCount = hitWrite = 0;
        }
        if (! started)
        {
            started = true;
            start = at;
            for (int k = 0; k < numLevels; ++k)
                written[(size_t) k] = at >> k;
        }
        auto& l = rings[0][0];
        auto& r = rings[0][1];
        const auto mask = (std::int64_t) sizes[0] - 1;
        for (int i = 0; i < n; ++i)
        {
            const auto slot = (size_t) (written[0] & mask);
            const auto a = left[i], b = right != nullptr ? right[i] : left[i];
            l[slot] = a;
            r[slot] = b;
            ++written[0];
            binPeak = juce::jmax (binPeak, std::abs (a), std::abs (b));
            if (++binFill >= binSamples)
            {
                const auto next = (shownWrite.load (std::memory_order_relaxed) + 1) % bins;
                shownBins[(size_t) next].store (juce::jmin (1.0f, binPeak), std::memory_order_relaxed);
                shownWrite.store (next, std::memory_order_relaxed);
                binFill = 0;
                binPeak = 0.0f;
            }
        }
        for (int k = 1; k < numLevels; ++k)
            halveUpTo (k);
    }

    void LiveTape::halveUpTo (int level) noexcept
    {
        // Copy `level` from the one above, as far as that one has the samples its filter looks ahead to: sample m is
        // the half-band filter round sample 2m above (`halve` in Deck.cpp, run as the sound arrives).
        const auto& h = kernels::halfBand();
        const auto side = ((int) h.size() - 1) / 2;
        const auto from = (size_t) level - 1, to = (size_t) level;
        const auto fromMask = (std::int64_t) sizes[from] - 1, toMask = (std::int64_t) sizes[to] - 1;
        while (2 * written[to] + side < written[from])
        {
            const auto m = written[to];
            for (int c = 0; c < 2; ++c)
            {
                const auto* x = rings[from][(size_t) c].data();
                double sum = 0.0;
                for (int n = -side; n <= side; ++n)
                    sum += h[(size_t) (n + side)] * x[(size_t) ((2 * m - n) & fromMask)];
                rings[to][(size_t) c][(size_t) (m & toMask)] = (float) sum;
            }
            ++written[to];
        }
    }

    void LiveTape::addHit (std::int64_t at) noexcept
    {
        hits[(size_t) hitWrite] = at;
        hitWrite = (hitWrite + 1) % maxHits;
        hitCount = juce::jmin (hitCount + 1, maxHits);
    }

    double LiveTape::newest() const noexcept
    {
        auto best = (double) written[0];
        for (int k = 0; k < numLevels; ++k)
            best = juce::jmin (best, (double) ((written[(size_t) k] - padding - 2) << k));
        return best - safety;
    }

    double LiveTape::oldest() const noexcept
    {
        // Never before this tape began (its first few ms on the slower copies hold a little of what was there).
        auto best = (double) (start + ((std::int64_t) (64 + padding) << (numLevels - 1)));
        for (int k = 0; k < numLevels; ++k)
            best = juce::jmax (best, (double) ((written[(size_t) k] - sizes[(size_t) k] + padding + 2) << k));
        return best + safety;
    }

    double LiveTape::hit (int n, double minimumAge) const noexcept
    {
        const auto low = oldest();
        for (int k = 0; k < hitCount; ++k)
        {
            const auto at = (double) hits[(size_t) ((hitWrite - 1 - k + maxHits) % maxHits)];
            if ((double) written[0] - at >= minimumAge && at >= low)
                if (n-- == 0)
                    return at;
        }
        return -1.0;
    }

    float LiveTape::read (int channel, double position, double pace) const noexcept
    {
        if (sizes[0] == 0)
            return 0.0f;
        const auto reading = kernels::readingFor (std::abs (pace), numLevels, padding);
        const auto p = position / reading.scale;
        const auto first = (std::int64_t) std::ceil (p - reading.reach), last = (std::int64_t) std::floor (p + reading.reach);
        const auto count = (int) (last - first + 1);
        const auto& ring = rings[(size_t) reading.level][(size_t) juce::jlimit (0, 1, channel)];
        const auto mask = (std::int64_t) sizes[(size_t) reading.level] - 1;
        const auto slot = (int) (first & mask);
        if (slot + count <= sizes[(size_t) reading.level])
            return kernels::sum (ring.data() + slot, first, count, p, reading.cutoff);
        // Round the ring's end: the samples copied out in order.
        float x[2 * padding + 8];
        for (int j = 0; j < count && j < (int) std::size (x); ++j)
            x[j] = ring[(size_t) ((first + j) & mask)];
        return kernels::sum (x, first, juce::jmin (count, (int) std::size (x)), p, reading.cutoff);
    }
}
