#include "Deck.h"
#include "DeckKernel.h"

#include <cmath>

namespace deck
{
    namespace
    {
        // The interpolator's kernel and the half-band filter: DeckKernel.h (shared with THE LIVE RECORD's tape).
        using kernels::zeroCrossings;
        using kernels::kernelTable;
        using kernels::kernel;
        using kernels::halfBand;

        // Half a band: `from` (padded) filtered to below half its Nyquist (halfBand) and every other sample kept,
        // into `to` (padded).
        void halve (const juce::AudioBuffer<float>& from, int fromLength, juce::AudioBuffer<float>& to, int toLength)
        {
            const auto& h = halfBand();
            const auto side = ((int) h.size() - 1) / 2;
            to.setSize (2, toLength + 2 * Record::padding);
            to.clear();
            for (int c = 0; c < 2; ++c)
            {
                const auto* x = from.getReadPointer (c) + Record::padding;
                auto* y = to.getWritePointer (c) + Record::padding;
                for (int m = 0; m < toLength; ++m)
                {
                    double sum = 0.0;
                    const auto lo = juce::jmax (-side, 2 * m - fromLength + 1), hi = juce::jmin (side, 2 * m);
                    for (int n = lo; n <= hi; ++n)
                        sum += h[(size_t) (n + side)] * x[2 * m - n];
                    y[m] = (float) sum;
                }
            }
        }

        // THE HOUSE RECORD's sounds (see Deck.h), synthesized into `out` from `start` seconds.
        void addVowel (juce::AudioBuffer<float>& out, double rate, double start)
        {
            // "Ahh": a voice's pulses as harmonics falling 6 dB an octave, shaped by the vowel's three formants
            // (about 730, 1090 and 2440 Hz), the pitch falling from 150 to 120 Hz with a little vibrato.
            constexpr double seconds = 0.9;
            const struct { double hz, width, gain; } formants[] = { { 730.0, 110.0, 1.0 }, { 1090.0, 130.0, 0.55 }, { 2440.0, 200.0, 0.3 } };
            const auto count = (int) (seconds * rate);
            std::vector<double> phases (96, 0.0);
            auto* l = out.getWritePointer (0);
            for (int i = 0; i < count; ++i)
            {
                const auto t = i / rate;
                const auto f0 = (150.0 - 30.0 * t / seconds) * (1.0 + 0.012 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.2 * t));
                const auto env = juce::jmin (1.0, t / 0.03) * juce::jmin (1.0, (seconds - t) / 0.15);
                double sum = 0.0;
                for (size_t h = 1; h < phases.size(); ++h)
                {
                    const auto f = f0 * (double) h;
                    if (f > 7000.0)
                        break;
                    double shape = 0.02;
                    for (const auto& formant : formants)
                        shape += formant.gain * std::exp (-std::pow ((f - formant.hz) / formant.width, 2.0));
                    phases[h] += f / rate;
                    sum += shape / (double) h * std::sin (2.0 * juce::MathConstants<double>::pi * phases[h]);
                }
                l[(int) (start * rate) + i] += (float) (sum * env);
            }
        }

        void addHorn (juce::AudioBuffer<float>& out, double rate, double start)
        {
            // A horn stab: A major (A3, E4, A4, C#5) as saws a little detuned, a bright blat that settles darker.
            constexpr double seconds = 0.45;
            const double notes[] = { 220.0, 329.63, 440.0, 554.37 };
            const auto count = (int) (seconds * rate);
            std::vector<double> phases (4 * 40, 0.0);
            auto* l = out.getWritePointer (0);
            for (int i = 0; i < count; ++i)
            {
                const auto t = i / rate;
                const auto cutoff = t < 0.04 ? 600.0 + 3400.0 * t / 0.04 : 1500.0 + 2500.0 * std::exp (-(t - 0.04) / 0.08);
                const auto env = juce::jmin (1.0, t / 0.008) * juce::jmin (1.0, (seconds - t) / 0.06);
                double sum = 0.0;
                for (size_t n = 0; n < 4; ++n)
                    for (size_t h = 1; h < 40; ++h)
                    {
                        const auto f = notes[n] * (1.0 + 0.002 * ((double) n - 1.5)) * (double) h;
                        if (f > 9000.0)
                            break;
                        auto& phase = phases[n * 40 + h];
                        phase += f / rate;
                        sum += std::exp (-f / cutoff) / (double) h * std::sin (2.0 * juce::MathConstants<double>::pi * phase);
                    }
                l[(int) (start * rate) + i] += (float) (0.32 * sum * env);
            }
        }

        void addBreak (juce::AudioBuffer<float>& out, double rate, double start, juce::Random& random)
        {
            // A short break at 95 BPM, sixteenths: kick on 1, the "and" of 2 and on 2's last sixteenth; snare on 2
            // and 4 (steps 4 and 10, of 12); a closed hat every eighth.
            const auto step = 60.0 / 95.0 / 4.0;
            auto* l = out.getWritePointer (0);
            const auto put = [&] (double at, double seconds, auto&& sound)
            {
                const auto first = (int) ((start + at) * rate);
                const auto count = (int) (seconds * rate);
                for (int i = 0; i < count && first + i < out.getNumSamples(); ++i)
                    l[first + i] += sound (i / rate);
            };
            for (int s : { 0, 6, 7 })
            {
                double phase = 0.0;
                put (s * step, 0.3, [&] (double t)
                {
                    phase += (50.0 + 100.0 * std::exp (-t / 0.03)) / rate;
                    return (float) (0.9 * std::exp (-t / 0.09) * std::sin (2.0 * juce::MathConstants<double>::pi * phase));
                });
            }
            for (int s : { 4, 10 })
            {
                double phase = 0.0, last = 0.0;
                put (s * step, 0.22, [&] (double t)
                {
                    phase += 190.0 / rate;
                    const auto noise = random.nextFloat() * 2.0 - 1.0;
                    const auto bright = noise - 0.6 * last;   // tilted up: a snare's rattle, not a hiss's flatness
                    last = noise;
                    return (float) (0.5 * std::exp (-t / 0.06) * bright + 0.35 * std::exp (-t / 0.03) * std::sin (2.0 * juce::MathConstants<double>::pi * phase));
                });
            }
            for (int s = 0; s < 12; s += 2)
            {
                double last = 0.0;
                put (s * step, 0.05, [&] (double t)
                {
                    const auto noise = random.nextFloat() * 2.0 - 1.0;
                    const auto high = noise - last;   // the highs only
                    last = noise;
                    return (float) (0.18 * std::exp (-t / 0.012) * high);
                });
            }
        }

        void addTone (juce::AudioBuffer<float>& out, double rate)
        {
            const auto count = (int) (houseToneSeconds * rate);
            auto* l = out.getWritePointer (0);
            for (int i = 0; i < count; ++i)
            {
                const auto t = i / rate;
                const auto env = juce::jmin (1.0, t / 0.01) * juce::jmin (1.0, (houseToneSeconds - t) / 0.01);
                l[(int) (houseToneStart * rate) + i] += (float) (0.5 * env * std::sin (2.0 * juce::MathConstants<double>::pi * houseToneHz * t));
            }
        }

        //======================================================================
        // THE NU RECORD (0.11.0): the sounds nu-metal's DJs scratched over the band, our own, synthesized here. The
        // owner's reference (a DJ part they like) had a scratched vocal, a distorted guitar stab and noise/squeal.
        constexpr double twoPiD = juce::MathConstants<double>::twoPi;

        // An overdriven stage: asymmetric (a valve's and a pedal's clipping isn't even), its offset taken back out.
        double drive (double x, double gain, double bias = 0.15)
        {
            return std::tanh (gain * x + bias) - std::tanh (bias);
        }

        // A biquad (RBJ's cookbook) run over a span, for the record's filters (a cabinet's top, a megaphone's band).
        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            static Biquad make (bool highPass, double hz, double q, double rate)
            {
                const auto w = twoPiD * hz / rate, alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
                Biquad f;
                f.b0 = (highPass ? (1.0 + c) / 2.0 : (1.0 - c) / 2.0) / a0;
                f.b1 = (highPass ? -(1.0 + c) : 1.0 - c) / a0;
                f.b2 = f.b0;
                f.a1 = -2.0 * c / a0;
                f.a2 = (1.0 - alpha) / a0;
                return f;
            }
            double run (double x)
            {
                const auto y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
        };

        // A plucked string (Karplus-Strong): a burst of noise round a loop as long as one cycle of `hz`, softened a
        // little each time round. `damp` 0: ringing; towards 1: a palm on the strings (dark, short).
        std::vector<double> pluck (double hz, double seconds, double rate, double damp, juce::Random& random)
        {
            const auto length = juce::jmax (2, (int) std::round (rate / hz));
            std::vector<double> loop ((size_t) length), out ((size_t) (seconds * rate));
            for (auto& v : loop)
                v = random.nextDouble() * 2.0 - 1.0;
            const auto keep = 0.4985 - 0.02 * damp, bright = 1.0 - 0.6 * damp;
            double last = 0.0;
            for (size_t i = 0; i < out.size(); ++i)
            {
                const auto at = i % (size_t) length;
                const auto next = loop[(at + 1) % (size_t) length];
                out[i] = loop[at];
                const auto averaged = keep * (loop[at] + next);
                loop[at] = bright * averaged + (1.0 - bright) * last;   // damped: darker each time round
                last = loop[at];
            }
            return out;
        }

        // The guitar's rig: boosted into hard asymmetric clipping, then a 4x12 cabinet's band (80 Hz to about 4.5 kHz).
        void amp (std::vector<double>& signal, double rate, double gain)
        {
            auto low = Biquad::make (true, 80.0, 0.7, rate), top1 = Biquad::make (false, 4500.0, 0.7, rate), top2 = Biquad::make (false, 5500.0, 0.6, rate);
            auto mids = Biquad::make (true, 400.0, 0.5, rate);   // the pedal's tight low end before the clipping
            for (auto& v : signal)
            {
                const auto tightened = v - 0.6 * (v - mids.run (v));
                v = top2.run (top1.run (low.run (drive (tightened, gain, 0.2))));
            }
        }

        void put (juce::AudioBuffer<float>& out, double rate, double start, const std::vector<double>& signal, double level = 1.0)
        {
            auto* l = out.getWritePointer (0);
            const auto first = (int) (start * rate);
            for (size_t i = 0; i < signal.size() && first + (int) i < out.getNumSamples(); ++i)
                l[first + (int) i] += (float) (level * signal[i]);
        }

        void addShout (juce::AudioBuffer<float>& out, double rate, double start)
        {
            // A shouted "AAH!": a voice pushed hard (its first formant raised, as a shout's is: about 850, 1250, 2800
            // Hz), pitch high and falling (290 to 230 Hz), a growl (a pulse every other cycle, the voice's fry), then
            // driven hard through a megaphone's band (300 Hz to 3.5 kHz): the distorted vocal a DJ cuts up.
            constexpr double seconds = 0.75;
            const struct { double hz, width, gain; } formants[] = { { 850.0, 130.0, 1.0 }, { 1250.0, 150.0, 0.75 }, { 2800.0, 260.0, 0.45 } };
            std::vector<double> voice ((size_t) (seconds * rate));
            std::vector<double> phases (64, 0.0);
            double growl = 0.0;
            for (size_t i = 0; i < voice.size(); ++i)
            {
                const auto t = (double) i / rate;
                const auto f0 = (290.0 - 60.0 * t / seconds) * (1.0 + 0.02 * std::sin (twoPiD * 7.0 * t));
                const auto env = juce::jmin (1.0, t / 0.015) * juce::jmin (1.0, (seconds - t) / 0.12);
                double sum = 0.0;
                for (size_t h = 1; h < phases.size(); ++h)
                {
                    const auto f = f0 * (double) h;
                    if (f > 6000.0)
                        break;
                    double shape = 0.03;
                    for (const auto& formant : formants)
                        shape += formant.gain * std::exp (-std::pow ((f - formant.hz) / formant.width, 2.0));
                    phases[h] += f / rate;
                    sum += shape / std::sqrt ((double) h) * std::sin (twoPiD * phases[h]);
                }
                growl += 0.5 * f0 / rate;
                sum *= 0.7 + 0.3 * std::sin (twoPiD * growl);   // every other cycle louder: the fry under a shout
                voice[i] = sum * env;
            }
            auto low = Biquad::make (true, 300.0, 0.7, rate), high = Biquad::make (false, 3500.0, 0.7, rate);
            double peak = 1.0e-9;
            for (auto v : voice)
                peak = juce::jmax (peak, std::abs (v));
            for (auto& v : voice)
                v = high.run (low.run (drive (v / peak, 5.0, 0.1)));
            put (out, rate, start, voice);
        }

        void addGuitarStab (juce::AudioBuffer<float>& out, double rate, double start, juce::Random& random)
        {
            // A drop-D power chord (D2, A2, D3) strummed down (8 ms between strings), let ring a moment and choked,
            // through the rig.
            constexpr double seconds = 1.1;
            std::vector<double> chord ((size_t) (seconds * rate), 0.0);
            const double strings[] = { 73.42, 110.0, 146.83 };
            for (int s = 0; s < 3; ++s)
            {
                const auto string = pluck (strings[s], seconds, rate, 0.0, random);
                const auto offset = (size_t) (0.008 * s * rate);
                for (size_t i = 0; i + offset < chord.size(); ++i)
                    chord[i + offset] += string[i];
            }
            for (size_t i = 0; i < chord.size(); ++i)
            {
                const auto t = (double) i / rate;
                chord[i] *= juce::jmin (1.0, (seconds - t) / 0.08);   // choked by the hand
            }
            amp (chord, rate, 9.0);
            put (out, rate, start, chord);
        }

        void addSqueal (juce::AudioBuffer<float>& out, double rate, double start, juce::Random& random)
        {
            // Feedback: a whine swelling in and rising (1.9 to 2.6 kHz) with a hand's vibrato, driven, with an edge of
            // hiss; then a laser zap, a square diving from 6 kHz to 150 Hz.
            constexpr double whine = 1.0, zap = 0.35, gap = 0.05;
            std::vector<double> sound ((size_t) ((whine + gap + zap) * rate), 0.0);
            double phase = 0.0;
            for (size_t i = 0; i < (size_t) (whine * rate); ++i)
            {
                const auto t = (double) i / rate;
                const auto hz = (1900.0 + 700.0 * t / whine) * (1.0 + 0.012 * std::sin (twoPiD * 6.0 * t));
                phase += hz / rate;
                const auto env = juce::jmin (1.0, t / 0.35) * juce::jmin (1.0, (whine - t) / 0.03);
                const auto hiss = 0.08 * (random.nextDouble() * 2.0 - 1.0);
                sound[i] = env * drive (std::sin (twoPiD * phase) + hiss, 2.5, 0.0);
            }
            phase = 0.0;
            const auto zapStart = (size_t) ((whine + gap) * rate);
            for (size_t i = 0; i < (size_t) (zap * rate); ++i)
            {
                const auto t = (double) i / rate;
                const auto hz = 6000.0 * std::pow (150.0 / 6000.0, t / zap);
                phase += hz / rate;
                const auto env = juce::jmin (1.0, t / 0.003) * juce::jmin (1.0, (zap - t) / 0.02);
                sound[zapStart + i] = 0.8 * env * drive (std::sin (twoPiD * phase), 4.0, 0.0);
            }
            put (out, rate, start, sound);
        }

        void addChugs (juce::AudioBuffer<float>& out, double rate, double start, juce::Random& random)
        {
            // Palm-muted D2 chugs, sixteenths at 120 BPM (seven), then an open D power chord to end on: the rhythm a
            // TRANSFORM cuts up.
            const auto step = 60.0 / 120.0 / 4.0;
            std::vector<double> riff ((size_t) (1.55 * rate), 0.0);
            for (int k = 0; k < 7; ++k)
            {
                auto chug = pluck (73.42, 0.16, rate, 0.85, random);
                const auto fifth = pluck (110.0, 0.16, rate, 0.85, random);
                for (size_t i = 0; i < chug.size(); ++i)
                    chug[i] = (chug[i] + 0.8 * fifth[i]) * std::exp (-(double) i / rate / 0.05);
                const auto at = (size_t) (k * step * rate);
                for (size_t i = 0; i < chug.size() && at + i < riff.size(); ++i)
                    riff[at + i] += 1.6 * chug[i];
            }
            const auto at = (size_t) (7 * step * rate);
            for (const auto hz : { 73.42, 110.0, 146.83 })
            {
                const auto string = pluck (hz, 0.6, rate, 0.0, random);
                for (size_t i = 0; i < string.size() && at + i < riff.size(); ++i)
                    riff[at + i] += string[i] * juce::jmin (1.0, (0.6 - (double) i / rate) / 0.06);
            }
            amp (riff, rate, 9.0);
            put (out, rate, start, riff);
        }

        // Each part brought to the same peak (0.7), so none is much louder than the others.
        void normalise (juce::AudioBuffer<float>& audio, double rate, double from, double to)
        {
            const auto first = (int) (from * rate), last = juce::jmin (audio.getNumSamples(), (int) (to * rate));
            const auto peak = audio.getMagnitude (0, first, last - first);
            if (peak > 0.0f)
                audio.applyGain (0, first, last - first, 0.7f / peak);
        }
    }

    std::shared_ptr<const Record> makeRecord (const juce::AudioBuffer<float>& audio, double sampleRate, const juce::String& name)
    {
        auto record = std::make_shared<Record>();
        record->name = name;
        record->sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        record->length = audio.getNumSamples();
        auto& top = record->levels[0];
        top.setSize (2, record->length + 1 + 2 * Record::padding);
        top.clear();
        if (audio.getNumChannels() > 0)
            for (int c = 0; c < 2; ++c)
                top.copyFrom (c, Record::padding, audio, juce::jmin (c, audio.getNumChannels() - 1), 0, record->length);
        auto fromLength = record->length + 1;
        for (int k = 1; k < Record::numLevels; ++k)
        {
            const auto toLength = (record->length >> k) + 1;
            halve (record->levels[(size_t) k - 1], fromLength, record->levels[(size_t) k], toLength);
            fromLength = toLength;
        }
        if (record->length > 0)
            for (int b = 0; b < Record::overviewBins; ++b)
            {
                const auto first = (int) ((std::int64_t) record->length * b / Record::overviewBins);
                const auto last = juce::jmax (first + 1, (int) ((std::int64_t) record->length * (b + 1) / Record::overviewBins));
                float peak = 0.0f;
                for (int c = 0; c < 2; ++c)
                    peak = juce::jmax (peak, top.getMagnitude (c, Record::padding + first, juce::jmin (last, record->length) - first));
                record->overview[(size_t) b] = juce::jmin (1.0f, peak);
            }
        return record;
    }

    std::shared_ptr<const Record> houseRecord()
    {
        static const std::shared_ptr<const Record> record = []
        {
            constexpr double rate = 48000.0;
            juce::AudioBuffer<float> audio (1, (int) (8.0 * rate));
            audio.clear();
            juce::Random random (1979);
            addVowel (audio, rate, 0.0);
            addHorn (audio, rate, 2.0);
            addBreak (audio, rate, 4.0, random);
            addTone (audio, rate);
            for (int part = 0; part < 3; ++part)
                normalise (audio, rate, 2.0 * part, 2.0 * part + 2.0);
            return makeRecord (audio, rate, "HOUSE RECORD");
        }();
        return record;
    }

    std::shared_ptr<const Record> nuRecord()
    {
        static const std::shared_ptr<const Record> record = []
        {
            constexpr double rate = 48000.0;
            juce::AudioBuffer<float> audio (1, (int) (8.0 * rate));
            audio.clear();
            juce::Random random (2001);
            addShout (audio, rate, 0.0);
            addGuitarStab (audio, rate, 2.0, random);
            addSqueal (audio, rate, 4.0, random);
            addChugs (audio, rate, 6.0, random);
            for (int part = 0; part < 4; ++part)
                normalise (audio, rate, 2.0 * part, 2.0 * part + 2.0);
            return makeRecord (audio, rate, "NU RECORD");
        }();
        return record;
    }

    std::shared_ptr<const Record> liveRecord()
    {
        static const std::shared_ptr<const Record> record = []
        {
            auto r = std::make_shared<Record>();
            r->name = "LIVE";
            r->live = true;
            r->sampleRate = 48000.0;
            r->length = (int) (LiveTape::seconds * r->sampleRate);
            return std::shared_ptr<const Record> (r);
        }();
        return record;
    }

    float faderGain (float fader, float curve, bool hamster) noexcept
    {
        const auto x = juce::jlimit (0.0f, 1.0f, hamster ? 1.0f - fader : fader);
        const auto open = 1.0f + (0.01f - 1.0f) * juce::jlimit (0.0f, 1.0f, curve);   // how far it travels to open fully
        if (x <= 0.0f)
            return 0.0f;
        if (x >= open)
            return 1.0f;
        return std::sin (x / open * juce::MathConstants<float>::halfPi);
    }

    double scratchBeats (int technique) noexcept
    {
        constexpr double beats[] = { 0.5, 0.5, 0.5, 0.5, 0.5, 1.0, 1.0, 0.5, 2.0, 1.0, 1.0, 1.0 };
        return beats[juce::jlimit (0, numScratches - 1, technique)];
    }

    // 0.8.0: the shapes tuned to how DJs really scratch, from K. F. Hansen's measurements of DJs' record and fader
    // movements (KTH: "The basics of scratching", 2002; "Analysis of a genuine scratch performance", 2003; Hansen,
    // Fabiani and Bresin 2009) and the techniques' usual descriptions:
    //  - the pull back is about 11% faster than the push (Hansen measured the backward strokes 11% faster on average),
    //    so every stroke pushes for 53% of its time and pulls for 47% (scratchPush);
    //  - CHIRP: "on every movement forward, the fader closes fast after the start, and backwards only the last bit of
    //    the sound is cut in" (Hansen): heard at the push's start and the pull's end, shut round the far turn (0.5.0
    //    cut both strokes' ends, which isn't a chirp);
    //  - TRANSFORMER and CRAB: the fader rests closed and is tapped open (a CRAB's thumb springs it back shut after
    //    each finger's tap), so they're short bursts, not half open (0.5.0 had them open half the time);
    //  - SCRIBBLE: a tensed forearm's tremor, 16 strokes a second at 120 BPM (0.5.0's 32 was faster than a hand).
    ScratchShape scratchAt (int technique, double p) noexcept
    {
        p = juce::jlimit (0.0, 1.0, p);
        constexpr auto pi = juce::MathConstants<double>::pi;
        constexpr auto push = scratchPush;
        const auto ease = [] (double x) { return (1.0 - std::cos (pi * juce::jlimit (0.0, 1.0, x))) * 0.5; };   // 0 to 1, smoothly
        const auto pushing = p < push;
        const auto half = pushing ? p / push : (p - push) / (1.0 - push);   // how far through this stroke (the push or the pull)
        const auto stroke = pushing ? ease (half) : 1.0 - ease (half);      // forward to the full length and back, smoothly
        switch (technique)
        {
            case 0:  return { stroke, true };                                   // BABY: forward and back, the fader open
            case 1:  return { stroke, pushing };                                // FORWARD: heard pushed, cut pulled back
            case 2:  return { stroke, ! pushing };                              // BACKWARD: cut pushed, heard pulled back
            case 3:  return { stroke, pushing ? half < 0.35 : half > 0.65 };    // CHIRP: the push's start, the pull's end
            case 4:  return { p < 0.2 ? ease (p / 0.2) : p < 0.5 ? 1.0 : ease (1.0 - (p - 0.5) / 0.5), p < 0.15 };   // STAB: a hard push, its start heard
            case 5:  return { p < 0.75 ? p / 0.75 : ease (1.0 - (p - 0.75) / 0.25), p < 0.75 };   // DRAG: a slow push heard, a quick pull cut
            case 6:  return { p < 0.3 ? ease (p / 0.3) : p < 0.5 ? 1.0 - 0.5 * ease ((p - 0.3) / 0.2)   // TEAR: pushed, then pulled back
                                       : p < 0.65 ? 0.5 : 0.5 * (1.0 - ease ((p - 0.65) / 0.35)), true };   // in two moves, a stop between
            case 7:  return { 0.12 * (1.0 - std::cos (2.0 * pi * 4.0 * p)) * 0.5, true };   // SCRIBBLE: four tiny tense strokes
            case 8:  return { stroke, std::fmod (p * 16.0, 1.0) < 0.35 };       // TRANSFORMER: a slow stroke, the fader tapped open 1/32s
            case 9:  return { stroke, ! (half > 0.42 && half < 0.58) };         // FLARE: open, one click shut in each stroke
            case 10: return { stroke, ! ((half > 0.28 && half < 0.38) || (half > 0.62 && half < 0.72)) };   // 2-CLICK FLARE
            case 11: return { stroke, half < 0.6 && std::fmod (half * 4.0 / 0.6, 1.0) < 0.4 };   // CRAB: four quick taps open on each stroke
            default: return {};
        }
    }

    //==============================================================================
    Deck::Deck()
    {
        kernelTable();   // built here, never on the audio thread
        halfBand();
    }

    void Deck::prepare (double hostSampleRate)
    {
        hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
        fadeLength = juce::jmax (1, (int) (0.004 * hostRate));
        gain.reset (hostRate, 0.03);
        mixer.prepare (hostRate);
        reset();
    }

    void Deck::reset()
    {
        position = speed = platter = 0.0;
        motorOn = spinning = holdKey = cutKey = transformKey = scratchHeld = false;
        scratchTechnique = -1;
        scratchOpen = true;
        shownScratch = -1;
        faderNow = 1.0f;
        shownFader = 1.0f;
        mixer.reset();
        hissL = hissR = 0.0;
        crackleNow = 0.0f;
        hand = Hand::none;
        wheel = 0.0f;
        wheelCentredFor = 0;
        fadeLeft = 0;
        thumpLevel = crackleLevel = 0.0f;
        shownPosition = 0.0;
        shownSpeed = 0.0f;
        shownMotor = false;
    }

    float Deck::read (const Record& record, int channel, double at, double rate) noexcept
    {
        // The copy whose band suits the speed: a record read at r samples per output sample needs its highs below
        // 1/r of its band, or they fold back down. Level k has 1/2^k of the band (and rate); between levels the
        // kernel widens (its cutoff lowered) for what's left, so the brightness falls smoothly as the speed rises.
        const auto pace = std::abs (rate);
        const auto level = pace > 1.0 ? juce::jmin (Record::numLevels - 1, (int) std::floor (std::log2 (pace))) : 0;
        const auto scale = (double) (1 << level);
        const auto p = at / scale;
        // The cutoff, as a fraction of this level's band; never so low the kernel would reach past the padding.
        const auto cutoff = juce::jlimit ((double) zeroCrossings / (Record::padding - 2), 1.0, scale / juce::jmax (pace, 1.0e-9));
        const auto reach = zeroCrossings / cutoff;
        const auto* table = kernelTable().data();
        const auto* x = record.levels[(size_t) level].getReadPointer (channel) + Record::padding;
        const auto first = (int) std::ceil (p - reach), last = (int) std::floor (p + reach);
        double sum = 0.0;
        for (int i = first; i <= last; ++i)
            sum += (double) kernel (table, (p - i) * cutoff) * x[i];
        return (float) (sum * cutoff);
    }

    void Deck::grab (Hand which) noexcept
    {
        hand = which;
        handAnchor = handSmooth1 = handSmooth2 = position;
        spinning = false;
        wheelCentredFor = 0;
    }

    void Deck::dropNeedle (double to, float velocity) noexcept
    {
        fadePosition = position;
        fadeLeft = fadeLength;
        const auto jump = to - position;
        position = to;
        handAnchor += jump;   // a hand on the record keeps holding it, at its new place
        handSmooth1 += jump;
        handSmooth2 += jump;
        thumpPhase = 0.0;
        thumpLevel = 0.5f * velocity;
        crackleLevel = 0.6f * velocity;
    }

    double Deck::liveDropPlace (int cue) const noexcept
    {
        // DROP 1 the newest key press, DROP 2 the one before, and so on (each 4 ms before its press, so the attack's
        // whole); none yet: half a second back.
        if (tape == nullptr)
            return position;
        const auto rate = tape->getSampleRate();
        const auto at = tape->hit (cue, 0.03 * rate);
        const auto place = at >= 0.0 ? at - 0.004 * rate : liveHigh - 0.5 * rate;
        return juce::jlimit (liveLow, juce::jmax (liveLow, liveHigh), place);
    }

    double Deck::liveScratchPlace (double strokeSamples) const noexcept
    {
        // As PR7-K's SCRATCH parks its needle: just before the newest key press far enough back for the stroke to
        // push the record forward without passing the newest sound; none: that far back and a quarter second more.
        if (tape == nullptr)
            return position;
        const auto rate = tape->getSampleRate();
        const auto need = strokeSamples + 0.03 * rate;
        const auto at = tape->hit (0, need);
        const auto place = at >= 0.0 && (double) tape->getEnd() - at <= 3.0 * rate ? at - 0.004 * rate
                                                                                 : liveHigh - need - 0.25 * rate;
        return juce::jlimit (liveLow, juce::jmax (liveLow, liveHigh), place);
    }

    void Deck::handleMidi (const juce::MidiMessage& message, const Record* record, const Settings& settings) noexcept
    {
        if (message.isController() && message.getControllerNumber() == 1)
        {
            // MOD HAND: the mod wheel moves the record from where it takes it (it stays where it's left: the hand lets
            // go once it's been still a moment, and takes the record again from there when it moves).
            const auto value = (float) message.getControllerValue() / 127.0f;
            if (settings.modHand && std::abs (value - modWheel) > 1.0e-6f && hand != Hand::mouse)
            {
                if (hand != Hand::modWheel)
                {
                    grab (Hand::modWheel);
                    modWheelAtGrab = modWheel;
                }
                modStillFor = 0;
            }
            modWheel = value;
            return;
        }
        if (message.isPitchWheel())
        {
            wheel = juce::jlimit (-1.0f, 1.0f, (float) (message.getPitchWheelValue() - 8192) / 8191.0f);
            if (std::abs (wheel) > 1.0e-6f)
            {
                wheelCentredFor = 0;
                if (hand == Hand::none)
                    grab (Hand::wheel);
            }
            return;
        }
        const auto key = message.getNoteNumber() - settings.firstKey;
        if (key < 0 || key >= numKeys)
        {
            // THE SCRATCH LIBRARY's keys: a technique starts (its stroke as long as the key is hard), or its key lets go.
            const auto technique = message.getNoteNumber() - settings.firstScratchKey;
            if (settings.firstScratchKey < 0 || technique < 0 || technique >= numScratches)
                return;
            if (message.isNoteOn() && hand != Hand::mouse)
            {
                if (record != nullptr && record->live && hand == Hand::none && std::abs (speed) < 0.05)
                {
                    const auto stroke = (double) settings.scratchReach * secondsPerTurn * (tape != nullptr ? tape->getSampleRate() : 48000.0);
                    dropNeedle (liveScratchPlace (stroke), 0.0f);
                    fadeLeft = 0;   // the record was still: nothing to fade out
                }
                grab (Hand::scratch);
                scratchTechnique = technique;
                scratchHeld = true;
                scratchPhase = 0.0;
                scratchSettling = 0;
                scratchOpen = true;
                scratchStroke = (double) settings.scratchReach * (0.4 + 0.6 * (double) message.getFloatVelocity());
            }
            else if (message.isNoteOff() && technique == scratchTechnique)
                scratchHeld = false;   // it finishes the stroke it's on
            return;
        }
        if (message.isNoteOff())
        {
            if (key == (int) Key::hold)
                holdKey = false;
            else if (key == (int) Key::cut)
                cutKey = false;
            else if (key == (int) Key::transform)
                transformKey = false;
            return;
        }
        if (! message.isNoteOn())
            return;
        switch ((Key) key)
        {
            case Key::play:     motorOn = true; spinning = false; break;
            case Key::stop:     motorOn = false; spinning = false; break;
            case Key::backspin:
                // Thrown back by hand: the motor let go (the record would only fight it), then slowing to a stop.
                spinning = true;
                spinElapsed = 0.0;
                motorOn = false;
                platter = 0.0;
                if (hand == Hand::wheel)
                    hand = Hand::none;
                break;
            case Key::hold:     holdKey = true; break;
            case Key::drop1: case Key::drop2: case Key::drop3: case Key::drop4:
            {
                const auto cue = key == (int) Key::drop1 ? 0 : key == (int) Key::drop2 ? 1 : key == (int) Key::drop3 ? 2 : 3;
                if (record != nullptr && record->live)
                    dropNeedle (liveDropPlace (cue), message.getFloatVelocity() * settings.needle);
                else
                {
                    const auto length = record != nullptr ? record->length : 0;
                    dropNeedle (juce::jlimit (0.0f, 1.0f, settings.cues[(size_t) cue]) * length, message.getFloatVelocity() * settings.needle);
                }
                motorOn = true;   // onto a turning record
                spinning = false;
                break;
            }
            case Key::cut:       cutKey = true; break;
            case Key::transform: transformKey = true; transformBeat = 0.0; break;
            default: break;   // the block's other keys: for the scratch library (stage 4)
        }
    }

    void Deck::process (float* left, float* right, int numSamples, const juce::MidiBuffer& midi, int firstSample,
                        const Record* record, const Settings& settings) noexcept
    {
        // THE LIVE RECORD (0.12.0): the tape, read between its oldest and newest sound (in the engine's clock).
        const auto* liveTape = record != nullptr && record->live ? tape : nullptr;
        const auto live = liveTape != nullptr;
        if (record != nullptr && record->live && liveTape == nullptr)
            record = nullptr;   // no tape (none set): silence
        const auto recordRate = live ? liveTape->getSampleRate() : record != nullptr ? record->sampleRate : 48000.0;
        auto low = 0.0, high = record != nullptr ? (double) record->length : 0.0;
        if (live)
        {
            low = liveTape->oldest();
            high = juce::jmax (low, liveTape->newest());
            if (! wasLive)
                position = high;   // put on: the needle at the newest sound
        }
        else if (wasLive)
            position = 0.0;   // a record put on in its place: from its start
        wasLive = live;
        liveLow = low;
        liveHigh = high;
        const auto length = high;   // (the far end; `low` the near one: 0 on a record)
        const auto ratio = recordRate / hostRate;          // the record's samples per output sample at its natural speed
        const auto base = (double) settings.baseSpeed();   // what the motor turns it at
        const auto startStep = base / juce::jmax (0.01, (double) settings.startSeconds) / hostRate;
        const auto brakeStep = base / juce::jmax (0.01, (double) settings.brakeSeconds) / hostRate;
        const auto towards = [this] (double ms) { return 1.0 - std::exp (-1.0 / (0.001 * ms * hostRate)); };
        const auto slipmat = towards (20.0);   // let go, the record catches the platter in about 20 ms
        const auto holding = towards (4.0);    // a hand stopping it
        const auto flick = towards (20.0);     // BACKSPIN: thrown back in about 20 ms
        const auto handFollow = towards (6.0); // the record following the hand (smoothing the wheel's steps)
        // THE CROSSFADER: where it's set (HAMSTER and CURVE in), and its moves as straight 1 ms lines: as sharp as a
        // scratch mixer's cut sounds, without the click a jump in one sample would make.
        const auto faderSet = faderGain (settings.fader, settings.curve, settings.hamster);
        // CUT and TRANSFORM (0.9.0): the deck's channel snapped to the other edge (open: cut; cut: open), a tap on
        // the fader. (0.4.0-0.8.0 mirrored the fader's place, the same at its ends; with the A/B fader in the middle,
        // where both decks are open, a mirror wouldn't move it.)
        const auto faderFlipped = faderSet >= 0.5f ? 0.0f : 1.0f;
        const auto faderStep = (float) (1.0 / (0.001 * hostRate));
        const auto transformCycle = transformBeats[juce::jlimit (0, (int) std::size (transformBeats) - 1, settings.transformRate)];
        const auto beatsPerSample = juce::jmax (1.0, settings.bpm) / 60.0 / hostRate;
        const auto turnSamples = secondsPerTurn * recordRate;
        const auto centredLetGo = (int) (0.05 * hostRate);
        const auto modLetGo = (int) (0.15 * hostRate);
        const auto wear = juce::jlimit (0.0f, 1.0f, settings.wear);
        const auto crackleFall = (float) std::exp (-1.0 / (0.0003 * hostRate));   // a crackle: 0.3 ms
        mixer.set (settings.mixer);
        const auto scratchSettle = (int) (0.03 * hostRate);
        // The key blocks moved (KEYS or SCRATCH KEYS changed): the held keys' note-offs will go elsewhere, so let go.
        if (settings.firstKey != keysWere || settings.firstScratchKey != scratchKeysWere)
        {
            if (keysWere != -1 || scratchKeysWere != -1)
                releaseKeys();
            keysWere = settings.firstKey;
            scratchKeysWere = settings.firstScratchKey;
        }
        const auto thumpStep = 2.0 * juce::MathConstants<double>::pi * 55.0 / hostRate;
        const auto thumpDecay = (float) std::exp (-1.0 / (0.035 * hostRate));
        const auto crackleDecay = (float) std::exp (-1.0 / (0.15 * hostRate));
        const auto crackleChance = (float) (900.0 / hostRate);   // about 900 clicks a second as it lands, dying away
        gain.setTargetValue (settings.levelDb <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (settings.levelDb));

        // The mouse takes the record, or lets it go (to the wheel if that's still off its centre).
        const auto mouse = mouseHolding.load();
        if (mouse && hand != Hand::mouse)
            grab (Hand::mouse);
        else if (! mouse && hand == Hand::mouse)
        {
            hand = Hand::none;
            if (std::abs (wheel) > 1.0e-6f)
                grab (Hand::wheel);
        }
        position = juce::jlimit (low, length, position);   // a shorter record put on (or the tape moved on)

        float blockPeak = 0.0f;
        auto events = midi.findNextSamplePosition (firstSample);
        const auto end = midi.findNextSamplePosition (firstSample + numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            for (; events != end && (*events).samplePosition <= firstSample + i; ++events)
                handleMidi ((*events).getMessage(), record, settings);

            // The motor: up to speed over START, down over BRAKE (both straight lines, as a turntable's are).
            const auto motorTarget = motorOn ? base : 0.0;
            platter = platter < motorTarget ? juce::jmin (motorTarget, platter + startStep) : juce::jmax (motorTarget, platter - brakeStep);

            if (hand != Hand::none)
            {
                // A hand moves the record: it goes where the hand puts it, and its speed is how fast that is.
                auto turns = 0.0;
                if (hand == Hand::mouse)
                    turns = (double) mouseTurns.load (std::memory_order_relaxed);
                else if (hand == Hand::wheel)
                    turns = (double) wheel * settings.reachTurns;
                else if (hand == Hand::modWheel)
                    turns = (double) (modWheel - modWheelAtGrab) * 2.0 * settings.reachTurns;   // the whole wheel: twice REACH
                else
                {
                    // A technique: its stroke and its fader where it is now; on through it at the song's tempo (times
                    // RATE); at its end again while its key's held, else the hand lets go and the motor takes it on.
                    const auto length = scratchBeats (scratchTechnique);
                    const auto shape = scratchAt (scratchTechnique, scratchPhase / length);
                    turns = shape.hand * scratchStroke;
                    scratchOpen = shape.open;
                    scratchPhase += beatsPerSample * scratchRates[juce::jlimit (0, (int) std::size (scratchRates) - 1, settings.scratchRate)];
                    if (scratchPhase >= length)
                    {
                        if (scratchHeld)
                            scratchPhase -= length;
                        else
                        {
                            // Done: the hand stays 30 ms at the stroke's end (where it took the record) while its
                            // smoothing settles, so the record is back home before the hand lets go.
                            scratchPhase = length;
                            if (++scratchSettling >= scratchSettle)
                            {
                                hand = Hand::none;
                                scratchTechnique = -1;
                                scratchOpen = true;
                            }
                        }
                    }
                }
                const auto target = handAnchor + turns * turnSamples;
                handSmooth1 += handFollow * (target - handSmooth1);
                handSmooth2 += handFollow * (handSmooth1 - handSmooth2);
                const auto next = juce::jlimit (low, length, handSmooth2);
                speed = (next - position) / ratio;
                position = next;
                if (hand == Hand::wheel)
                {
                    wheelCentredFor = std::abs (wheel) > 1.0e-6f ? 0 : wheelCentredFor + 1;
                    if (wheelCentredFor >= centredLetGo)
                        hand = Hand::none;   // the wheel back at its centre a moment: the hand lets go
                }
                else if (hand == Hand::modWheel && ++modStillFor >= modLetGo)
                    hand = Hand::none;       // the mod wheel still a moment: the hand lets go where it is
            }
            else
            {
                if (holdKey)
                    speed += holding * (0.0 - speed);
                else if (spinning)
                {
                    spinElapsed += 1.0 / hostRate;
                    const auto t = spinElapsed / juce::jmax (0.05, (double) settings.spinSeconds);
                    const auto target = t < 1.0 ? -(double) settings.spinSpeed * base * (1.0 - t) : 0.0;
                    speed += flick * (target - speed);
                    if (t >= 1.0 && std::abs (speed) < 1.0e-4)
                    {
                        spinning = false;
                        speed = 0.0;
                    }
                }
                else
                    speed += slipmat * (platter - speed);
                speed = juce::jlimit (-16.0, 16.0, speed);
                // WEAR's warp: the record's speed wobbling once a turn (+-0.4% at full wear), by where the needle is in
                // the turn (a warp is a place on the record).
                const auto warp = wear > 0.0f ? 1.0 + 0.004 * wear * std::sin (juce::MathConstants<double>::twoPi * position / turnSamples) : 1.0;
                position = juce::jlimit (low, length, position + speed * ratio * warp);
            }

            const auto rate = speed * ratio;
            const auto loudness = (float) juce::jlimit (-2.0, 2.0, speed / juce::jmax (0.01, base));   // the cartridge: level with speed
            const auto level = gain.getNextValue();
            float outL = 0.0f, outR = 0.0f;
            if (record != nullptr && length > low && std::abs (loudness) > 1.0e-6f)
            {
                outL = live ? liveTape->read (0, position, rate) : read (*record, 0, position, rate);
                outR = live ? liveTape->read (1, position, rate) : read (*record, 1, position, rate);
            }
            if (fadeLeft > 0)
            {
                // A jump: the old place fades out under the new.
                const auto f = (float) fadeLeft / (float) fadeLength;
                float oldL = 0.0f, oldR = 0.0f;
                if (record != nullptr && length > low)
                {
                    oldL = live ? liveTape->read (0, fadePosition, rate) : read (*record, 0, fadePosition, rate);
                    oldR = live ? liveTape->read (1, fadePosition, rate) : read (*record, 1, fadePosition, rate);
                }
                outL = outL * (1.0f - f) + oldL * f;
                outR = outR * (1.0f - f) + oldR * f;
                fadePosition = juce::jlimit (low, length, fadePosition + rate);
                --fadeLeft;
            }
            outL *= loudness;
            outR *= loudness;

            // WEAR: the record's surface noise, in the groove, so it moves with the record: as loud and as bright as
            // the record's going fast (a scratch hisses up, a held record is silent), and crackle as often.
            if (wear > 0.0f)
            {
                const auto moving = juce::jmin (2.0, std::abs (speed));
                if (moving > 1.0e-4)
                {
                    const auto bright = juce::jmin (1.0, juce::MathConstants<double>::twoPi * 6000.0 * moving / hostRate);
                    hissL += bright * ((double) wearRandom.nextFloat() * 2.0 - 1.0 - hissL);
                    hissR += bright * ((double) wearRandom.nextFloat() * 2.0 - 1.0 - hissR);
                    const auto hiss = (float) (0.03 * wear * moving);
                    outL += (float) hissL * hiss;
                    outR += (float) hissR * hiss;
                    if (wearRandom.nextFloat() < (float) (wear * moving * 25.0 / hostRate))   // 25 a second at full wear
                        crackleNow = wear * (0.05f + 0.25f * wearRandom.nextFloat()) * (wearRandom.nextBool() ? 1.0f : -1.0f);
                }
                outL += crackleNow;
                outR += crackleNow * 0.8f;
                crackleNow *= crackleFall;
            }

            // The needle landing.
            if (thumpLevel > 1.0e-5f || crackleLevel > 1.0e-5f)
            {
                auto noise = thumpLevel * (float) std::sin (thumpPhase);
                thumpPhase += thumpStep;
                thumpLevel *= thumpDecay;
                if (random.nextFloat() < crackleChance)
                    noise += crackleLevel * (random.nextFloat() * 2.0f - 1.0f);
                crackleLevel *= crackleDecay;
                outL += noise;
                outR += noise;
            }

            // THE CROSSFADER: CUT flips it while held (open to cut, or cut to open: a tap); TRANSFORM flips it for the
            // second half of each cycle, in time with the song (or, the song stopped, from when it was pressed).
            auto flipped = cutKey;
            if (transformKey)
            {
                const auto beat = settings.ppq >= 0.0 ? settings.ppq + (double) i * beatsPerSample : transformBeat;
                transformBeat += beatsPerSample;
                if (std::fmod (beat, transformCycle) >= 0.5 * transformCycle)
                    flipped = ! flipped;
            }
            // A technique's own cuts: the fader shut while it has it cut.
            const auto faderTarget = (hand == Hand::scratch && ! scratchOpen) ? 0.0f : flipped ? faderFlipped : faderSet;
            faderNow = faderNow < faderTarget ? juce::jmin (faderTarget, faderNow + faderStep) : juce::jmax (faderTarget, faderNow - faderStep);

            // THE SOUND (0.6.0): the cartridge and the mixer channel, after LEVEL, before the crossfader (exactly as
            // it was while they're neutral).
            outL *= level;
            outR *= level;
            mixer.process (outL, outR);
            const auto heardL = outL * faderNow, heardR = outR * faderNow;
            left[i] += heardL;
            if (right != nullptr)
                right[i] += heardR;
            blockPeak = juce::jmax (blockPeak, std::abs (heardL), std::abs (heardR));
        }
        // Events past the block (none, normally) still count.
        for (; events != end; ++events)
            handleMidi ((*events).getMessage(), record, settings);

        shownPosition.store (position / recordRate, std::memory_order_relaxed);
        shownBehind.store (live ? juce::jmax (0.0, ((double) liveTape->getEnd() - position) / recordRate) : 0.0, std::memory_order_relaxed);
        shownSpeed.store ((float) speed, std::memory_order_relaxed);
        shownMotor.store (motorOn, std::memory_order_relaxed);
        shownFader.store (faderNow, std::memory_order_relaxed);
        shownScratch.store (hand == Hand::scratch ? scratchTechnique : -1, std::memory_order_relaxed);
        for (auto was = peak.load (std::memory_order_relaxed); blockPeak > was && ! peak.compare_exchange_weak (was, blockPeak, std::memory_order_relaxed);) {}
    }
}
