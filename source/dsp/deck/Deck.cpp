#include "Deck.h"

#include <cmath>

namespace deck
{
    namespace
    {
        // THE INTERPOLATOR's kernel: a sinc, windowed (Kaiser, beta 8: about 80 dB down outside the band) to 8 zero
        // crossings either side, tabled at 512 points per crossing and read by straight lines between them.
        constexpr int zeroCrossings = 8;
        constexpr int tableResolution = 512;
        constexpr double kaiserBeta = 8.0;

        double besselI0 (double x) noexcept
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

        const std::vector<float>& kernelTable()
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

        // Half a band: `from` (padded) filtered to below half its Nyquist and every other sample kept, into `to` (padded).
        // The filter: a windowed sinc, 127 taps (Kaiser beta 9, about 90 dB down), its cutoff at 0.454 of the band so
        // its stopband starts at the new Nyquist (nothing folds back as it's halved); flat to about 0.41 of the band.
        // Centred (it looks ahead as far as behind), so every copy lines up with the sample.
        const std::vector<double>& halfBand()
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
        reset();
    }

    void Deck::reset()
    {
        position = speed = platter = 0.0;
        motorOn = spinning = holdKey = false;
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

    void Deck::handleMidi (const juce::MidiMessage& message, const Record* record, const Settings& settings) noexcept
    {
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
            return;
        if (message.isNoteOff())
        {
            if (key == (int) Key::hold)
                holdKey = false;
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
                const auto length = record != nullptr ? record->length : 0;
                dropNeedle (juce::jlimit (0.0f, 1.0f, settings.cues[(size_t) cue]) * length, message.getFloatVelocity() * settings.needle);
                motorOn = true;   // onto a turning record
                spinning = false;
                break;
            }
            default: break;   // the block's other keys: for later stages
        }
    }

    void Deck::process (float* left, float* right, int numSamples, const juce::MidiBuffer& midi, int firstSample,
                        const Record* record, const Settings& settings) noexcept
    {
        const auto recordRate = record != nullptr ? record->sampleRate : 48000.0;
        const auto length = record != nullptr ? (double) record->length : 0.0;
        const auto ratio = recordRate / hostRate;          // the record's samples per output sample at its natural speed
        const auto base = (double) settings.baseSpeed();   // what the motor turns it at
        const auto startStep = base / juce::jmax (0.01, (double) settings.startSeconds) / hostRate;
        const auto brakeStep = base / juce::jmax (0.01, (double) settings.brakeSeconds) / hostRate;
        const auto towards = [this] (double ms) { return 1.0 - std::exp (-1.0 / (0.001 * ms * hostRate)); };
        const auto slipmat = towards (20.0);   // let go, the record catches the platter in about 20 ms
        const auto holding = towards (4.0);    // a hand stopping it
        const auto flick = towards (20.0);     // BACKSPIN: thrown back in about 20 ms
        const auto handFollow = towards (6.0); // the record following the hand (smoothing the wheel's steps)
        const auto turnSamples = secondsPerTurn * recordRate;
        const auto centredLetGo = (int) (0.05 * hostRate);
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
        if (position > length)
            position = length;   // a shorter record put on

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
                const auto turns = hand == Hand::mouse ? (double) mouseTurns.load (std::memory_order_relaxed) : (double) wheel * settings.reachTurns;
                const auto target = handAnchor + turns * turnSamples;
                handSmooth1 += handFollow * (target - handSmooth1);
                handSmooth2 += handFollow * (handSmooth1 - handSmooth2);
                const auto next = juce::jlimit (0.0, length, handSmooth2);
                speed = (next - position) / ratio;
                position = next;
                if (hand == Hand::wheel)
                {
                    wheelCentredFor = std::abs (wheel) > 1.0e-6f ? 0 : wheelCentredFor + 1;
                    if (wheelCentredFor >= centredLetGo)
                        hand = Hand::none;   // the wheel back at its centre a moment: the hand lets go
                }
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
                position = juce::jlimit (0.0, length, position + speed * ratio);
            }

            const auto rate = speed * ratio;
            const auto loudness = (float) juce::jlimit (-2.0, 2.0, speed / juce::jmax (0.01, base));   // the cartridge: level with speed
            const auto level = gain.getNextValue();
            float outL = 0.0f, outR = 0.0f;
            if (record != nullptr && length > 0.0 && std::abs (loudness) > 1.0e-6f)
            {
                outL = read (*record, 0, position, rate);
                outR = read (*record, 1, position, rate);
            }
            if (fadeLeft > 0)
            {
                // A jump: the old place fades out under the new.
                const auto f = (float) fadeLeft / (float) fadeLength;
                float oldL = 0.0f, oldR = 0.0f;
                if (record != nullptr && length > 0.0)
                {
                    oldL = read (*record, 0, fadePosition, rate);
                    oldR = read (*record, 1, fadePosition, rate);
                }
                outL = outL * (1.0f - f) + oldL * f;
                outR = outR * (1.0f - f) + oldR * f;
                fadePosition = juce::jlimit (0.0, length, fadePosition + rate);
                --fadeLeft;
            }
            outL *= loudness;
            outR *= loudness;

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

            left[i] += outL * level;
            if (right != nullptr)
                right[i] += outR * level;
        }
        // Events past the block (none, normally) still count.
        for (; events != end; ++events)
            handleMidi ((*events).getMessage(), record, settings);

        shownPosition.store (position / recordRate, std::memory_order_relaxed);
        shownSpeed.store ((float) speed, std::memory_order_relaxed);
        shownMotor.store (motorOn, std::memory_order_relaxed);
    }
}
