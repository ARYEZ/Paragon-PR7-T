// THE DECK's parameters (PR7-T 0.2.0, version hint 44: hosts need hints that only ever rise, so PR7-T carries on
// from PR7-K's 43 though its own version started again at 0.1.0). Defaults from deck::Settings, so the deck's tests
// and the plugin agree.
#include "Parameters.h"
#include "dsp/deck/Deck.h"

namespace
{
    using Range = juce::NormalisableRange<float>;
    using ToText = std::function<juce::String (float, int)>;
    Range skewed (float low, float high, float centre)
    {
        Range range { low, high };
        range.setSkewForCentre (centre);
        return range;
    }

    const ToText asSeconds = [] (float s, int) { return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s"; };
    const ToText asPercent = [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; };
}

namespace
{
    // One deck's parameters (0.9.0: deck A's or DECK B's, by `ids`), in deck A's order since 0.2.0, with the mixer's
    // shared ones (the crossfader, MOD HAND) between where they've always been, on deck A only. `hint` gives a
    // parameter's version hint from the one deck A's had (DECK B's are all 49: new in 0.9.0).
    void addDeck (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const ParamIDs::DeckIds& ids, const juce::String& name,
                  bool withShared, int keysDefault, int scratchKeysDefault, const std::function<int (int)>& hint)
    {
        const deck::Settings d;
        const auto pid = [&hint] (const char* id, int was) { return juce::ParameterID { id, hint (was) }; };
        const auto withText = [] (std::function<juce::String (float, int)> f) { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)); };
        const auto addFloat = [&] (const char* id, const juce::String& label, Range range, float value, const ToText& toText, int was)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid (id, was), name + label, range, value, withText (toText)));
        };
        layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids.on, 44), name + "On", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.keys, 44), name + "Keys", ParamIDs::deckKeyNames, keysDefault));
        addFloat (ids.pitch, "Pitch", { -16.0f, 16.0f, 0.01f }, d.pitch * 100.0f, [] (float pct, int)
        {
            return (pct > 0.004f ? "+" : "") + juce::String (pct, 2) + "%";
        }, 44);
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.speed, 44), name + "Speed", ParamIDs::deckSpeedNames, d.speed45 ? 1 : 0));
        addFloat (ids.start, "Start", skewed (0.05f, 5.0f, 0.7f), d.startSeconds, asSeconds, 44);
        addFloat (ids.brake, "Brake", skewed (0.05f, 10.0f, 1.2f), d.brakeSeconds, asSeconds, 44);
        addFloat (ids.spin, "Spin", { 1.0f, 8.0f, 0.01f }, d.spinSpeed, [] (float x, int) { return juce::String (x, 1) + "x"; }, 44);
        addFloat (ids.spinTime, "Spin Time", skewed (0.2f, 4.0f, 1.0f), d.spinSeconds, asSeconds, 44);
        addFloat (ids.reach, "Reach", { 0.05f, 1.0f, 0.01f }, d.reachTurns, [] (float turns, int) { return juce::String (turns, 2) + " turn"; }, 44);
        addFloat (ids.needle, "Needle", { 0.0f, 1.0f, 0.01f }, d.needle, asPercent, 44);
        addFloat (ids.level, "Level", { -60.0f, 6.0f, 0.1f }, d.levelDb, [] (float db, int)
        {
            return db <= -59.9f ? juce::String ("-inf") : (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
        }, 44);
        // THE CROSSFADER (0.4.0): its own hint, 45. The mixer's: on deck A's list only. 0.9.0: the A/B fader, A at 0
        // (its left), B at 1, the middle by default (with the default CUT curve: both decks fully open).
        if (withShared)
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid (ParamIDs::deckFader, 45), "Deck Fader", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
                0.5f, withText ([] (float x, int)
                {
                    return x <= 0.0005f ? juce::String ("A") : x >= 0.9995f ? juce::String ("B") : std::abs (x - 0.5f) < 0.0005f ? juce::String ("MIDDLE")
                         : x < 0.5f ? "A " + juce::String (juce::roundToInt ((0.5f - x) * 200.0f)) + "%" : "B " + juce::String (juce::roundToInt ((x - 0.5f) * 200.0f)) + "%";
                })));
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid (ParamIDs::deckCurve, 45), "Deck Curve", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, d.curve,
                withText ([] (float x, int)
                {
                    return x <= 0.005f ? juce::String ("SMOOTH") : x >= 0.995f ? juce::String ("CUT") : juce::String (juce::roundToInt (x * 100.0f)) + "%";
                })));
            layout.add (std::make_unique<juce::AudioParameterBool> (pid (ParamIDs::deckHamster, 45), "Deck Hamster", d.hamster));
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ParamIDs::deckTransform, 45), "Deck Transform", deck::transformNames, d.transformRate));
        }
        // THE SCRATCH LIBRARY (0.5.0): its own hint, 46.
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.scratchKeys, 46), name + "Scratch Keys", ParamIDs::deckScratchKeyNames, scratchKeysDefault));
        addFloat (ids.scratchReach, "Scratch Reach", { 0.05f, 1.0f, 0.01f }, d.scratchReach, [] (float turns, int) { return juce::String (turns, 2) + " turn"; }, 46);
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.scratchRate, 46), name + "Scratch Rate", deck::scratchRateNames, d.scratchRate));
        if (withShared)
            layout.add (std::make_unique<juce::AudioParameterBool> (pid (ParamIDs::deckModHand, 46), "Deck Mod Hand", d.modHand));
        // THE SOUND (0.6.0): its own hint, 47.
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.cart, 47), name + "Cart", deck::cartNames, d.mixer.cart));
        const char* bands[] = { "Low", "Mid", "High" };
        for (int band = 0; band < 3; ++band)
        {
            Range range { deck::MixerSettings::eqFloorDb, 6.0f, 0.1f };
            range.setSkewForCentre (-6.0f);   // the knob's middle at -6 dB: most of its travel where the ear works
            addFloat (ids.eq[band], juce::String ("EQ ") + bands[band], range, 0.0f, [] (float db, int)
            {
                return db <= deck::MixerSettings::eqFloorDb + 0.5f ? juce::String ("OFF") : (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
            }, 47);
            layout.add (std::make_unique<juce::AudioParameterBool> (pid (ids.kill[band], 47), name + "Kill " + bands[band], false));
        }
        addFloat (ids.filter, "Filter", { -1.0f, 1.0f, 0.001f }, 0.0f, [] (float x, int)
        {
            if (std::abs (x) <= 0.01f)
                return juce::String ("OFF");
            const auto hz = deck::DeckMixer::filterHz (x);
            return (x < 0.0f ? "LP " : "HP ") + (hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0, 1) + " kHz");
        }, 47);
        addFloat (ids.filterRes, "Filter Res", { 0.0f, 1.0f, 0.01f }, d.mixer.resonance, [] (float x, int) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; }, 47);
        addFloat (ids.wear, "Wear", { 0.0f, 1.0f, 0.01f }, d.wear, [] (float x, int)
        {
            return x <= 0.004f ? juce::String ("NEW") : juce::String (juce::roundToInt (x * 100.0f)) + "%";
        }, 47);
        // ROUTING (0.7.0): its own hint, 48. Nothing sent and no bus by default, so older projects play as before.
        addFloat (ids.sendDelay, "Send Delay", { 0.0f, 1.0f, 0.01f }, 0.0f, asPercent, 48);
        addFloat (ids.sendReverb, "Send Reverb", { 0.0f, 1.0f, 0.01f }, 0.0f, asPercent, 48);
        layout.add (std::make_unique<juce::AudioParameterChoice> (pid (ids.insert, 48), name + "Insert", ParamIDs::insertBusNames, 0));
        for (int cue = 0; cue < 4; ++cue)
            addFloat (ids.cues[cue], "Cue " + juce::String (cue + 1), { 0.0f, 1.0f, 0.001f }, d.cues[(size_t) cue],
                      [] (float x, int) { return juce::String (x * 100.0f, 1) + "%"; }, 44);
    }
}

void addDeckParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    addDeck (layout, ParamIDs::deckIds[0], "Deck ", true, ParamIDs::deckKeysDefault, ParamIDs::deckScratchKeysDefault, [] (int was) { return was; });
    // THE SECOND DECK (0.9.0): its own hint, 49; and HAND, which deck the wheels move.
    addDeck (layout, ParamIDs::deckIds[1], "Deck B ", false, ParamIDs::deckBKeysDefault, ParamIDs::deckBScratchKeysDefault, [] (int) { return 49; });
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::deckHand, 49 }, "Deck Hand", ParamIDs::deckHandNames, 0));
}
