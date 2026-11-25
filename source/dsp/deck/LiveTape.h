#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <vector>

namespace deck
{
    // THE LIVE RECORD's tape (0.12.0, the owner's ask: "add the live record"): the last `seconds` of what the synth
    // played, kept as it plays, so a deck can scratch it with nothing loaded or sampled first, as PR7-K's SCRATCH
    // cuts up what was just played. Our own design.
    //
    // What goes on it is the synth itself (every oscillator, the ones going to an insert bus too) before its effects,
    // and never a deck: a deck scratching its own tape would scratch its scratches. The deck's own mixer, routing and
    // the effects then shape what it scratches, as a DJ's mixer shapes a record.
    //
    // It's read as a record is: the sample at full band and copies filtered to 1/2, 1/4 and 1/8 of it for fast
    // strokes, here made as it records (the same half-band filter, run as the sound arrives, so each copy is a few
    // milliseconds behind the newest sound: `newest` says how far the needle may go). Places on it are the engine's
    // sample clock, so key presses (`addHit`) land where they were played.
    //
    // `write` and `read` are for the audio thread (nothing allocated); `prepare` allocates.
    class LiveTape
    {
    public:
        static constexpr double seconds = 12.0;   // how much it keeps
        static constexpr int numLevels = 4;
        static constexpr int maxHits = 16;

        void prepare (double sampleRate);
        void reset() noexcept;

        // `n` samples the synth played, the first at clock `at` (right may be null: mono). A gap or a jump back (the
        // host restarted) starts the tape afresh.
        void write (const float* left, const float* right, int n, std::int64_t at) noexcept;
        // A key press at clock `at` (keys and arps).
        void addHit (std::int64_t at) noexcept;

        double getSampleRate() const noexcept { return rate; }
        bool hasSound() const noexcept { return started && newest() > oldest(); }
        std::int64_t getEnd() const noexcept { return written[0]; }   // the clock just past the newest sample
        // Where the needle may be, in clock samples: from the oldest sound kept to the newest every copy has.
        double newest() const noexcept;
        double oldest() const noexcept;
        // The `n`th newest key press (0 the newest) at least `minimumAge` samples before the newest sound (-1: none).
        double hit (int n, double minimumAge) const noexcept;

        // Side `channel` at `position` (clock samples), as it sounds moving at `pace` samples of it per output sample.
        float read (int channel, double position, double pace) const noexcept;

        // For the page: the loudest sample in each of `bins` slices of the last `seconds`, newest last.
        static constexpr int bins = 600;
        float getBin (int b) const noexcept
        {
            const auto newestBin = shownWrite.load (std::memory_order_relaxed);   // the slot written last
            return shownBins[(size_t) ((newestBin + 1 + juce::jlimit (0, bins - 1, b)) % bins)].load (std::memory_order_relaxed);
        }

    private:
        static constexpr int padding = 40;     // as a record's: the interpolator reaches at most this far
        static constexpr int safety = 48;      // kept clear of the copies' newest and oldest samples
        void halveUpTo (int level) noexcept;

        double rate = 48000.0;
        std::array<int, numLevels> sizes {};                              // each copy's ring (a power of two)
        std::array<std::array<std::vector<float>, 2>, numLevels> rings;
        std::array<std::int64_t, numLevels> written {};                   // each copy: the index just past its newest
        std::int64_t start = 0;                                           // the clock where this tape began
        bool started = false;

        std::array<std::int64_t, maxHits> hits {};
        int hitCount = 0, hitWrite = 0;

        // The page's picture.
        int binSamples = 1, binFill = 0;
        float binPeak = 0.0f;
        std::array<std::atomic<float>, bins> shownBins {};
        std::atomic<int> shownWrite { bins - 1 };
    };
}
