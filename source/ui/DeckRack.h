#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// PR7-T 0.4.0: THE DECK's controls as one of the family's rack units across the top of the page (the owner: "let's
// bring in the pr7i rack style panel for all the knobs like we've done with the other instruments"): PR7-I's
// GuitarRack (0.5.0), drawn with the same parts as PR7-G's units: the pearl black chassis with the red light round it
// (PedalBody::paintUnit), the PARAGON PR7-T badge, stamped lettering and printed red lines, the orange LED screens,
// the big knobs with their values on small screens, the small keys with neon stripes, a neon status bar. Always pearl
// black (Finish::paintAs), whatever the finish, as PR7-I's is on its dark page.
//
// Six pages (0.7.0: in two rows of three), chosen by the PAGE keys under the badge: DECK (the turntable: PITCH, RPM, START, BRAKE, SPIN, SPIN
// TIME, REACH, NEEDLE), FADER (the crossfader: FADER, CURVE, TRANSFORM's rate, HAMSTER, and a screen of it at work),
// CUES (CUE 1-4, the KEYS menu and the key map) and SCRATCH (0.5.0: the scratch library's REACH, RATE, MOD HAND, its
// keys' menu and a screen of its techniques to click), and MIXER (0.6.0: the cartridge, the mixer channel's EQ and
// KILLs, its FILTER, the record's WEAR) and ROUTING (0.7.0: the deck's sends to DELAY and REVERB, its insert bus).
// 0.9.0: it shows one deck at a time (A or B, its keys under the screen; the FADER page is the mixer's, shared).
// Left to right: the badge and the page keys; the screen (the
// record, how it's turning, where the needle is) over the A, B, ON, LOAD and HOUSE keys; the page; DECK OUT (the deck's own
// meter and LEVEL).
//
// Only the unit's face and its page keys: the controls on it (DeckPage's) are its children, laid out in the areas it
// gives, shown and hidden by DeckPage as the page changes (onPageChanged).
class DeckRack final : public juce::Component
{
public:
    DeckRack();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    struct Screen
    {
        juce::String caption, record, state, line1, line2;
        bool problem = false;   // line2 says what's wrong with the record (a missing file)
        bool operator== (const Screen&) const = default;
    };
    void setScreen (const Screen&);
    const Screen& getScreen() const noexcept { return screen; }
    void setOn (bool);
    void setMeterDb (float);                         // the deck's own level, for the meter screen (dB)
    void setFaderGain (float);                       // the crossfader's gain now, for the FADER page's screen
    void setFaderInfo (const juce::StringArray&);    // the FADER screen's lines (its keys)
    void setKeyMap (const juce::StringArray&);       // the CUES screen's lines (the 12 keys)
    void setRoutingInfo (const juce::StringArray&);  // the ROUTING screen's lines (0.7.0)
    // SCRATCH (0.5.0): the techniques' screen: a line each ("C2  BABY"), the one playing lit (-1: none). Clicking one
    // plays it (held while the mouse is down): onScratchPressed (technique, down).
    void setScratchList (const juce::StringArray&, int playing);
    std::function<void (int technique, bool down)> onScratchPressed;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int scratchAt (juce::Point<int>) const;           // the technique on the screen at a point (-1: none)

    enum class Page { deck, fader, cues, scratch, mixer, routing };   // (0.5.0: SCRATCH; 0.6.0: MIXER; 0.7.0: ROUTING)
    void setPage (Page);
    Page getPage() const noexcept { return page; }
    std::function<void()> onPageChanged;
    juce::TextButton& getPageKey (Page p) noexcept
    {
        return p == Page::deck ? deckKey : p == Page::fader ? faderKey : p == Page::cues ? cuesKey : p == Page::scratch ? scratchKey
             : p == Page::mixer ? mixerKey : routingKey;
    }

    // Where the controls go, in the unit's pixels.
    juce::Rectangle<int> knobCell (int index) const;       // the page's knobs in a row (8 on DECK, the first 4 on the others)
    juce::Rectangle<int> killCell (int band) const;        // MIXER: the KILL keys, stacked in knobCell 4
    juce::Rectangle<int> faderScreenArea() const;          // FADER: the crossfader at work
    juce::Rectangle<int> menuCell() const;                 // CUES: the KEYS menu (its screen); SCRATCH: SCRATCH KEYS
    juce::Rectangle<int> keyMapArea() const;               // CUES: the key map's screen; SCRATCH: the techniques
    juce::Rectangle<int> keyCell (int index) const;        // 0 ON, 1 LOAD, 2 HOUSE
    juce::Rectangle<int> deckKeyCell (int deck) const;     // 0.9.0: DECK A, DECK B (which deck the rack shows)
    juce::Rectangle<int> sideKnobCell() const;             // LEVEL
    juce::Rectangle<int> screenArea() const;
    juce::Rectangle<int> meterArea() const;

    static constexpr int numKnobs = 8;
    static constexpr int preferredHeight = 260;

private:
    juce::Rectangle<float> face() const;
    juce::Rectangle<float> content() const;
    juce::Rectangle<float> badgeColumn() const;
    juce::Rectangle<float> displayColumn() const;
    juce::Rectangle<float> pageArea() const;     // under the page's title
    juce::Rectangle<float> sideColumn() const;
    juce::Rectangle<int> pageKeyCell (int index) const;
    juce::Rectangle<int> underScreenCell (int index) const;
    void paintScreen (juce::Graphics&);
    void paintMeter (juce::Graphics&);
    void paintFaderScreen (juce::Graphics&);
    void paintKeyMap (juce::Graphics&);
    void paintScratchList (juce::Graphics&);
    void paintRoutingScreen (juce::Graphics&);
    juce::Rectangle<float> scratchRow (int index) const;

    juce::Image satin;
    Screen screen;
    bool on = true;
    float meterDb = -100.0f, faderGain = 1.0f;
    juce::StringArray faderInfo, keyMap, scratchList, routingInfo;
    int scratchPlaying = -1, scratchClicked = -1;
    Page page = Page::deck;
    juce::TextButton deckKey, faderKey, cuesKey, scratchKey, mixerKey, routingKey;
};
