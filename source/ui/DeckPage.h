#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// THE DECK's page (PR7-T 0.2.0): the turntable.
//  - THE TURNTABLE (0.3.0): our own deck in the PR7 family's "Tron" look (as PR7-D's kit, PR7-I's guitar and PR7-O's
//    strings), rendered in Blender (resources/deck/): gloss black, its neon edge running pink to red to orange. The
//    platter turns as the record does and the arm moves in as it plays through; the neon brightens as it turns. Grab
//    the record with the mouse and it follows your hand: drag round it to move it forwards or back, hold still to stop
//    it, let go and the motor takes it again.
//  - The record: its sound as a strip, the needle's place on it, and the four cue points DROP 1-4 land on.
//  - The keys: PLAY, STOP, BACKSPIN, HOLD (held), DROP 1-4, the same as the deck's 12 keys on a keyboard.
//  - The knobs: the pitch fader, 33 / 45, the motor's START and BRAKE, BACKSPIN's throw and time, the wheel's REACH,
//    the needle's sound, the cues and the deck's level.
class DeckPlatter final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    explicit DeckPlatter (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh() { timerCallback(); }

    // Where the record is on the component (the picture fitted and centred in it), for the hand and for tests.
    juce::Point<float> getPlatterCentre() const;
    float getRecordRadius() const;
    float getArmAngle() const noexcept { return armAngle; }   // radians the arm has swung in from the record's start
    static constexpr float pictureAspect = 1400.0f / 1080.0f;
    static std::vector<juce::Image> picturesForTest();   // the deck, lit, the platter, the arm

private:
    void timerCallback() override;
    float angleAt (juce::Point<float>) const;
    juce::Rectangle<float> pictureArea() const;
    void prepare (float scale);
    ParagonX1AudioProcessor& processor;
    double shownTurns = 0.0;
    float shownLit = 0.0f, armAngle = 0.0f;
    bool grabbed = false;
    float lastAngle = 0.0f, handTurns = 0.0f;
    // The pictures shrunk once to the screen's pixels (PR7-D's kit view: resampling them on every repaint was its
    // CPU load); made again only when the size or the screen's scale changes.
    struct Shown { float scale = 0.0f; juce::Rectangle<float> area; juce::Image deck, lit, platter, arm; } shown;
};

class DeckStrip final : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Timer
{
public:
    explicit DeckStrip (ParagonX1AudioProcessor&);
    void paint (juce::Graphics&) override;
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;
    ParagonX1AudioProcessor& processor;
    double shownSeconds = 0.0;
};

class DeckPage final : public juce::Component,
                       private juce::Timer
{
public:
    explicit DeckPage (ParagonX1AudioProcessor&);
    ~DeckPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<ParamControl*> getShownControls();
    DeckPlatter& getPlatter() noexcept { return platter; }
    // Reads the deck now (what the page's timers do), for tests and snapshots that run no message loop.
    void refreshForTest() { timerCallback(); platter.refresh(); strip.refresh(); }

private:
    void timerCallback() override;
    void press (deck::Key, bool down);
    void refreshRecord();

    ParagonX1AudioProcessor& processor;
    DeckPlatter platter;
    DeckStrip strip;
    std::vector<std::unique_ptr<ParamControl>> top, motor, needle;
    struct KeyButton { deck::Key key; std::unique_ptr<juce::TextButton> button; };
    std::vector<KeyButton> keys;
    juce::TextButton loadButton { "LOAD RECORD..." }, houseButton { "HOUSE RECORD" };
    juce::Label recordLabel, speedLabel, keysLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownRecord;
};
