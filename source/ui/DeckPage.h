#pragma once

#include "PluginProcessor.h"
#include "ui/SynthControls.h"

// THE DECK's page (PR7-T 0.2.0): the turntable.
//  - The platter, turning as the record does (the label's mark shows where the needle is in the turn). Grab it with
//    the mouse and the record follows your hand: drag round it to move the record forwards or back, hold still to
//    stop it, let go and the motor takes it again.
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

private:
    void timerCallback() override;
    float angleAt (juce::Point<float>) const;
    ParagonX1AudioProcessor& processor;
    double shownTurns = 0.0;
    float shownSpeed = 0.0f;
    bool grabbed = false;
    float lastAngle = 0.0f, handTurns = 0.0f;
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
