// THE DECK's parameters (PR7-T 0.2.0, version hint 44: hosts need hints that only ever rise, so PR7-T carries on
// from PR7-K's 43 though its own version started again at 0.1.0). Defaults from deck::Settings, so the deck's tests
// and the plugin agree.
#include "Parameters.h"
#include "dsp/deck/Deck.h"

namespace
{
    using Range = juce::NormalisableRange<float>;
    using ToText = std::function<juce::String (float, int)>;
    constexpr int v = 44;

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name, Range range,
                                                          float defaultValue, const ToText& toText)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, v }, name, range, defaultValue,
                                                            juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText));
    }

    Range skewed (float low, float high, float centre)
    {
        Range range { low, high };
        range.setSkewForCentre (centre);
        return range;
    }

    const ToText asSeconds = [] (float s, int) { return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s"; };
    const ToText asPercent = [] (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; };
}

void addDeckParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    const deck::Settings d;
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::deckOn, v }, "Deck On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::deckKeys, v }, "Deck Keys",
                                                              ParamIDs::deckKeyNames, ParamIDs::deckKeysDefault));
    layout.add (makeFloat (ParamIDs::deckPitch, "Deck Pitch", { -16.0f, 16.0f, 0.01f }, d.pitch * 100.0f, [] (float pct, int)
    {
        return (pct > 0.004f ? "+" : "") + juce::String (pct, 2) + "%";
    }));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::deckSpeed, v }, "Deck Speed",
                                                              ParamIDs::deckSpeedNames, d.speed45 ? 1 : 0));
    layout.add (makeFloat (ParamIDs::deckStart, "Deck Start", skewed (0.05f, 5.0f, 0.7f), d.startSeconds, asSeconds));
    layout.add (makeFloat (ParamIDs::deckBrake, "Deck Brake", skewed (0.05f, 10.0f, 1.2f), d.brakeSeconds, asSeconds));
    layout.add (makeFloat (ParamIDs::deckSpin, "Deck Spin", { 1.0f, 8.0f, 0.01f }, d.spinSpeed, [] (float x, int) { return juce::String (x, 1) + "x"; }));
    layout.add (makeFloat (ParamIDs::deckSpinTime, "Deck Spin Time", skewed (0.2f, 4.0f, 1.0f), d.spinSeconds, asSeconds));
    layout.add (makeFloat (ParamIDs::deckReach, "Deck Reach", { 0.05f, 1.0f, 0.01f }, d.reachTurns, [] (float turns, int)
    {
        return juce::String (turns, 2) + " turn";
    }));
    layout.add (makeFloat (ParamIDs::deckNeedle, "Deck Needle", { 0.0f, 1.0f, 0.01f }, d.needle, asPercent));
    layout.add (makeFloat (ParamIDs::deckLevel, "Deck Level", { -60.0f, 6.0f, 0.1f }, d.levelDb, [] (float db, int)
    {
        return db <= -59.9f ? juce::String ("-inf") : (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
    }));
    // THE CROSSFADER (0.4.0): its own hint, 45.
    {
        const auto v45 = [] (const char* id) { return juce::ParameterID { id, 45 }; };
        layout.add (std::make_unique<juce::AudioParameterFloat> (v45 (ParamIDs::deckFader), "Deck Fader", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, d.fader,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                return x <= 0.0005f ? juce::String ("CUT") : x >= 0.9995f ? juce::String ("OPEN") : juce::String (juce::roundToInt (x * 100.0f)) + "%";
            })));
        layout.add (std::make_unique<juce::AudioParameterFloat> (v45 (ParamIDs::deckCurve), "Deck Curve", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, d.curve,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
            {
                return x <= 0.005f ? juce::String ("SMOOTH") : x >= 0.995f ? juce::String ("CUT") : juce::String (juce::roundToInt (x * 100.0f)) + "%";
            })));
        layout.add (std::make_unique<juce::AudioParameterBool> (v45 (ParamIDs::deckHamster), "Deck Hamster", d.hamster));
        layout.add (std::make_unique<juce::AudioParameterChoice> (v45 (ParamIDs::deckTransform), "Deck Transform", deck::transformNames, d.transformRate));
    }
    // THE SCRATCH LIBRARY (0.5.0): its own hint, 46.
    {
        const auto v46 = [] (const char* id) { return juce::ParameterID { id, 46 }; };
        layout.add (std::make_unique<juce::AudioParameterChoice> (v46 (ParamIDs::deckScratchKeys), "Deck Scratch Keys",
                                                                  ParamIDs::deckScratchKeyNames, ParamIDs::deckScratchKeysDefault));
        layout.add (std::make_unique<juce::AudioParameterFloat> (v46 (ParamIDs::deckScratchReach), "Deck Scratch Reach",
            juce::NormalisableRange<float> { 0.05f, 1.0f, 0.01f }, d.scratchReach,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float turns, int) { return juce::String (turns, 2) + " turn"; })));
        layout.add (std::make_unique<juce::AudioParameterChoice> (v46 (ParamIDs::deckScratchRate), "Deck Scratch Rate", deck::scratchRateNames, d.scratchRate));
        layout.add (std::make_unique<juce::AudioParameterBool> (v46 (ParamIDs::deckModHand), "Deck Mod Hand", d.modHand));
    }
    // THE SOUND (0.6.0): its own hint, 47.
    {
        const auto v47 = [] (const char* id) { return juce::ParameterID { id, 47 }; };
        const auto withText = [] (std::function<juce::String (float, int)> f) { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f)); };
        layout.add (std::make_unique<juce::AudioParameterChoice> (v47 (ParamIDs::deckCart), "Deck Cart", deck::cartNames, d.mixer.cart));
        const char* bands[] = { "Low", "Mid", "High" };
        for (int band = 0; band < 3; ++band)
        {
            juce::NormalisableRange<float> range { deck::MixerSettings::eqFloorDb, 6.0f, 0.1f };
            range.setSkewForCentre (-6.0f);   // the knob's middle at -6 dB: most of its travel where the ear works
            layout.add (std::make_unique<juce::AudioParameterFloat> (v47 (ParamIDs::deckEq[band]), juce::String ("Deck EQ ") + bands[band], range, 0.0f,
                withText ([] (float db, int) { return db <= deck::MixerSettings::eqFloorDb + 0.5f ? juce::String ("OFF") : (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB"; })));
            layout.add (std::make_unique<juce::AudioParameterBool> (v47 (ParamIDs::deckKill[band]), juce::String ("Deck Kill ") + bands[band], false));
        }
        layout.add (std::make_unique<juce::AudioParameterFloat> (v47 (ParamIDs::deckFilter), "Deck Filter", juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f,
            withText ([] (float x, int)
            {
                if (std::abs (x) <= 0.01f)
                    return juce::String ("OFF");
                const auto hz = deck::DeckMixer::filterHz (x);
                return (x < 0.0f ? "LP " : "HP ") + (hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + " Hz" : juce::String (hz / 1000.0, 1) + " kHz");
            })));
        layout.add (std::make_unique<juce::AudioParameterFloat> (v47 (ParamIDs::deckFilterRes), "Deck Filter Res", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
            d.mixer.resonance, withText ([] (float x, int) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; })));
        layout.add (std::make_unique<juce::AudioParameterFloat> (v47 (ParamIDs::deckWear), "Deck Wear", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
            d.wear, withText ([] (float x, int) { return x <= 0.004f ? juce::String ("NEW") : juce::String (juce::roundToInt (x * 100.0f)) + "%"; })));
    }
    // ROUTING (0.7.0): its own hint, 48. Nothing sent and no bus by default, so older projects play as before.
    {
        const auto v48 = [] (const char* id) { return juce::ParameterID { id, 48 }; };
        const auto asPercent = [] (float x, int) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; };
        layout.add (std::make_unique<juce::AudioParameterFloat> (v48 (ParamIDs::deckSendDelay), "Deck Send Delay", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
                                                                 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (asPercent)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (v48 (ParamIDs::deckSendReverb), "Deck Send Reverb", juce::NormalisableRange<float> { 0.0f, 1.0f, 0.01f },
                                                                 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (asPercent)));
        layout.add (std::make_unique<juce::AudioParameterChoice> (v48 (ParamIDs::deckInsert), "Deck Insert", ParamIDs::insertBusNames, 0));
    }
    for (int cue = 0; cue < 4; ++cue)
        layout.add (makeFloat (ParamIDs::deckCues[cue], "Deck Cue " + juce::String (cue + 1), { 0.0f, 1.0f, 0.001f },
                               d.cues[(size_t) cue], [] (float x, int) { return juce::String (x * 100.0f, 1) + "%"; }));
}
