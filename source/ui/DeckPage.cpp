#include "DeckPage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    // A note's name as the on-screen keyboard has them (middle C, note 60, is C4).
    juce::String noteName (int note)
    {
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    }

    void styleButton (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        button.setColour (juce::TextButton::buttonOnColourId, ParagonColours::accentDim);
        button.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        button.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
    }
}

//==============================================================================
DeckPlatter::DeckPlatter (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The platter. Grab the record and drag round it to scratch: clockwise moves it forwards, back moves it "
                "back, holding still stops it. Let go and the motor takes it again. The label's mark turns with the "
                "record; the arm moves in as the needle goes through the record.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    startTimerHz (30);
}

void DeckPlatter::timerCallback()
{
    shownTurns = processor.getDeck().getPositionSeconds() / deck::secondsPerTurn;
    shownSpeed = processor.getDeck().getSpeed();
    repaint();
}

float DeckPlatter::angleAt (juce::Point<float> point) const
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    return std::atan2 (point.x - centre.x, centre.y - point.y);   // clockwise from twelve o'clock
}

void DeckPlatter::mouseDown (const juce::MouseEvent& e)
{
    grabbed = true;
    handTurns = 0.0f;
    lastAngle = angleAt (e.position);
    processor.setDeckMouseHand (true, 0.0f);
}

void DeckPlatter::mouseDrag (const juce::MouseEvent& e)
{
    const auto angle = angleAt (e.position);
    auto turned = angle - lastAngle;   // the shorter way round
    if (turned > juce::MathConstants<float>::pi)
        turned -= twoPi;
    else if (turned < -juce::MathConstants<float>::pi)
        turned += twoPi;
    lastAngle = angle;
    handTurns += turned / twoPi;
    processor.setDeckMouseHand (true, handTurns);
}

void DeckPlatter::mouseUp (const juce::MouseEvent&)
{
    grabbed = false;
    processor.setDeckMouseHand (false, 0.0f);
}

void DeckPlatter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto radius = size * 0.5f - 4.0f;
    const auto rotation = (float) (std::fmod (shownTurns, 1.0) * twoPi);
    const auto at = [centre] (float r, float angle) { return centre + juce::Point<float> (r * std::sin (angle), -r * std::cos (angle)); };
    const auto circle = [centre] (float r) { return juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (centre); };

    // The platter: brushed dark metal, its rim's strobe dots turning with it.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a262c), centre.x, centre.y - radius,
                                             juce::Colour (0xff0e0c0f), centre.x, centre.y + radius, false));
    g.fillEllipse (circle (radius));
    g.setColour (ParagonColours::panelEdge.brighter (0.3f));
    g.drawEllipse (circle (radius).reduced (0.5f), 1.0f);
    for (int dot = 0; dot < 90; ++dot)
    {
        const auto p = at (radius - 7.0f, rotation + twoPi * (float) dot / 90.0f);
        g.setColour (juce::Colour (0xff8a848c).withAlpha (dot % 2 == 0 ? 0.7f : 0.35f));
        g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre (p));
    }

    // The record: black vinyl, its grooves, a sheen that stays put as it turns under the light.
    const auto vinyl = radius * 0.9f;
    g.setColour (juce::Colour (0xff050405));
    g.fillEllipse (circle (vinyl));
    for (auto r = vinyl * 0.4f; r < vinyl - 2.0f; r += 2.5f)
    {
        g.setColour (juce::Colours::white.withAlpha (std::fmod (r, 7.5f) < 2.5f ? 0.06f : 0.025f));
        g.drawEllipse (circle (r), 0.6f);
    }
    {
        juce::Path sheen;
        sheen.addPieSegment (circle (vinyl), -0.9f, -0.35f, 0.42f);
        sheen.addPieSegment (circle (vinyl), juce::MathConstants<float>::pi - 0.9f, juce::MathConstants<float>::pi - 0.35f, 0.42f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (sheen);
    }

    // The label, in the PR7 colours, and its mark: where the record is in its turn.
    const auto label = vinyl * 0.36f;
    g.setGradientFill (juce::ColourGradient (ParagonColours::glow, centre.x - label, centre.y - label,
                                             ParagonColours::accent, centre.x + label, centre.y + label, false));
    g.fillEllipse (circle (label));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine ({ at (label * 0.18f, rotation), at (label * 0.95f, rotation) }, 4.0f);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (rotation, centre.x, centre.y));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.setFont (juce::FontOptions (label * 0.3f, juce::Font::bold | juce::Font::italic));
    g.drawText ("PR7-T", juce::Rectangle<float> (label * 1.6f, label * 0.4f).withCentre (centre.translated (0.0f, label * 0.48f)),
                juce::Justification::centred, false);
    g.restoreState();
    g.setColour (ParagonColours::metalMid);
    g.fillEllipse (circle (juce::jmax (3.0f, radius * 0.025f)));

    // The arm: from its pivot at the top right, the needle moving in as it plays through the record.
    const auto record = processor.getDeckRecord();
    const auto through = record != nullptr && record->seconds() > 0.0
                       ? (float) juce::jlimit (0.0, 1.0, processor.getDeck().getPositionSeconds() / record->seconds()) : 0.0f;
    const auto pivot = juce::Point<float> (centre.x + radius * 0.88f, centre.y - radius * 0.88f);
    const auto needleRadius = vinyl * (0.92f - 0.5f * through);
    // The needle where a circle of that radius round the platter's centre meets the arm's reach from the pivot.
    const auto armLength = radius * 1.2f;
    const auto d = pivot.getDistanceFrom (centre);
    const auto a = (d * d - armLength * armLength + needleRadius * needleRadius) / (2.0f * d);
    const auto h = std::sqrt (juce::jmax (0.0f, needleRadius * needleRadius - a * a));
    const auto towards = (pivot - centre) / d;
    const auto mid = centre + towards * a;
    const auto needlePoint = mid + juce::Point<float> (towards.y, -towards.x) * h;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine ({ pivot.translated (3.0f, 4.0f), needlePoint.translated (3.0f, 4.0f) }, 6.0f);
    g.setColour (ParagonColours::metalDark);
    g.drawLine ({ pivot, needlePoint }, 5.0f);
    g.setColour (ParagonColours::metalLight);
    g.drawLine ({ pivot, needlePoint }, 1.5f);
    g.setColour (ParagonColours::ink);
    g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 22.0f).withCentre (needlePoint), 2.0f);
    g.setColour (ParagonColours::metalMid);
    g.fillEllipse (juce::Rectangle<float> (radius * 0.16f, radius * 0.16f).withCentre (pivot));
    g.setColour (ParagonColours::ink);
    g.fillEllipse (juce::Rectangle<float> (radius * 0.07f, radius * 0.07f).withCentre (pivot));

    // A hand on the record: its edge lit.
    if (grabbed)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.8f));
        g.drawEllipse (circle (vinyl).reduced (1.0f), 2.5f);
    }
}

//==============================================================================
DeckStrip::DeckStrip (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("The record: its sound from start to end, the needle's place on it (the line), and the cue points DROP 1-4 "
                "land on (CUE 1-4 below).");
    startTimerHz (30);
}

void DeckStrip::timerCallback()
{
    shownSeconds = processor.getDeck().getPositionSeconds();
    repaint();
}

void DeckStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto record = processor.getDeckRecord();
    auto area = bounds.reduced (14.0f, 10.0f);
    auto labels = area.removeFromTop (18.0f);
    if (record == nullptr || record->length <= 0)
        return;

    // The sound, as a bar a slice (its loudest), dimmer behind the needle.
    const auto mid = area.getCentreY();
    const auto through = (float) juce::jlimit (0.0, 1.0, shownSeconds / record->seconds());
    const auto bins = (int) record->overview.size();
    const auto w = area.getWidth() / (float) bins;
    for (int b = 0; b < bins; ++b)
    {
        const auto x = area.getX() + (float) b * w;
        const auto h = juce::jmax (1.0f, record->overview[(size_t) b] * area.getHeight() * 0.48f);
        g.setColour ((x < area.getX() + through * area.getWidth() ? ParagonColours::accentDim : ParagonColours::accent).withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (x, mid - h, juce::jmax (1.0f, w), 2.0f * h));
    }

    // The cue points.
    const auto settings = processor.getDeckSettings();
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (size_t cue = 0; cue < settings.cues.size(); ++cue)
    {
        const auto x = area.getX() + settings.cues[cue] * area.getWidth();
        g.setColour (ParagonColours::text.withAlpha (0.55f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        juce::Path flag;
        flag.addTriangle (x, labels.getBottom(), x - 5.0f, labels.getY() + 4.0f, x + 5.0f, labels.getY() + 4.0f);
        g.setColour (ParagonColours::text);
        g.fillPath (flag);
        g.drawText (juce::String ((int) cue + 1), juce::Rectangle<float> (x + 6.0f, labels.getY(), 20.0f, labels.getHeight()),
                    juce::Justification::centredLeft, false);
    }

    // The needle.
    const auto x = area.getX() + through * area.getWidth();
    g.setColour (ParagonColours::glow);
    g.fillRect (juce::Rectangle<float> (x - 1.0f, area.getY() - 2.0f, 2.0f, area.getHeight() + 4.0f));
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (juce::String (shownSeconds, 2) + " / " + juce::String (record->seconds(), 2) + " S",
                labels.withTrimmedLeft (labels.getWidth() - 140.0f), juce::Justification::centredRight, false);
}

//==============================================================================
DeckPage::DeckPage (ParagonX1AudioProcessor& p)
    : processor (p), platter (p), strip (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip));
    };
    make (top, ParamIDs::deckOn, "ON", "THE DECK on or off. On, its 12 keys (KEYS) and the pitch wheel play the deck, not the "
                                       "synth; off, the synth has them all, as on PR7-K.");
    make (top, ParamIDs::deckKeys, "KEYS", "The octave the deck's 12 keys start on (C1: the on-screen keyboard's lowest).");
    make (motor, ParamIDs::deckPitch, "PITCH", "The pitch fader: the record's speed, and with it its pitch, up to 16% either way.");
    make (motor, ParamIDs::deckSpeed, "RPM", "33 or 45: 45 plays the record 1.35 times as fast (about 5 semitones up).");
    make (motor, ParamIDs::deckStart, "START", "How long the motor takes to bring a stopped record up to speed (PLAY).");
    make (motor, ParamIDs::deckBrake, "BRAKE", "How long the record takes to stop (STOP). Long: a slow tape-stop wind-down.");
    make (motor, ParamIDs::deckSpin, "SPIN", "BACKSPIN: how fast the record is thrown back, times its normal speed.");
    make (motor, ParamIDs::deckSpinTime, "SPIN TIME", "BACKSPIN: how long it takes to slow to a stop.");
    make (motor, ParamIDs::deckReach, "REACH", "How far the pitch wheel moves the record, all the way up or down, in turns of "
                                               "the platter (a turn: 1.8 seconds of the record).");
    make (needle, ParamIDs::deckNeedle, "NEEDLE", "How loud the needle's landing is on a DROP: its thump and crackle "
                                                  "(louder for a harder key).");
    for (int cue = 0; cue < 4; ++cue)
        make (needle, ParamIDs::deckCues[cue], "CUE " + juce::String (cue + 1),
              "Where DROP " + juce::String (cue + 1) + " puts the needle, from the record's start (0%) to its end (100%).");
    make (needle, ParamIDs::deckLevel, "LEVEL", "The deck's level, before the rack (EQ, DOMINION...), OUTPUT and the LIMITER, "
                                                "which it goes through as the synth does.");
    for (auto* list : { &top, &motor, &needle })
        for (auto& c : *list)
            addAndMakeVisible (*c);

    const std::pair<deck::Key, const char*> shown[] = {
        { deck::Key::play, "Starts the motor: the record comes up to speed over START." },
        { deck::Key::stop, "Stops the motor: the record winds down over BRAKE." },
        { deck::Key::backspin, "Throws the record back (SPIN), slowing to a stop over SPIN TIME. PLAY or a DROP starts it again." },
        { deck::Key::hold, "A hand holding the record still while it's held down; let go and the record catches up with the "
                           "platter at once (stabs: hold, DROP, let go)." },
        { deck::Key::drop1, "Drops the needle at CUE 1, the motor on." },
        { deck::Key::drop2, "Drops the needle at CUE 2, the motor on." },
        { deck::Key::drop3, "Drops the needle at CUE 3, the motor on." },
        { deck::Key::drop4, "Drops the needle at CUE 4, the motor on." } };
    for (const auto& [key, tip] : shown)
    {
        auto button = std::make_unique<juce::TextButton> (deck::keyNames[(int) key]);
        styleButton (*button);
        button->setTooltip (tip);
        if (key == deck::Key::hold)
        {
            auto* b = button.get();
            b->onStateChange = [this, b, down = false] () mutable
            {
                if (b->isDown() != down)
                {
                    down = b->isDown();
                    press (deck::Key::hold, down);
                }
            };
        }
        else
        {
            button->setTriggeredOnMouseDown (true);
            button->onClick = [this, key = key] { press (key, true); press (key, false); };
        }
        addAndMakeVisible (*button);
        keys.push_back ({ key, std::move (button) });
    }

    styleButton (loadButton);
    styleButton (houseButton);
    loadButton.setTooltip ("Puts a record on the deck: a sample from a file (WAV, AIFF, FLAC or Ogg, up to 3 minutes). "
                           "The project remembers it.");
    houseButton.setTooltip ("Puts THE HOUSE RECORD on the deck, our own built-in scratch record: an \"ahh\", a horn stab, "
                            "a drum break and a 1 kHz tone, two seconds apart, at the cues' first places.");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Put a record on the deck", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& c)
                              {
                                  if (c.getResult().existsAsFile())
                                      processor.loadDeckRecord (c.getResult());
                                  refreshRecord();
                              });
    };
    houseButton.onClick = [this] { processor.useHouseRecord(); refreshRecord(); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (houseButton);

    for (auto* label : { &recordLabel, &speedLabel, &keysLabel })
    {
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (*label);
    }
    recordLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    speedLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    speedLabel.setJustificationType (juce::Justification::centred);
    speedLabel.setColour (juce::Label::textColourId, ParagonColours::accent);
    keysLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));

    addAndMakeVisible (platter);
    addAndMakeVisible (strip);
    refreshRecord();
    timerCallback();
    startTimerHz (10);
}

DeckPage::~DeckPage()
{
    processor.setDeckMouseHand (false, 0.0f);
}

void DeckPage::press (deck::Key key, bool down)
{
    const auto note = processor.getDeckSettings().firstKey + (int) key;
    if (down)
        processor.getKeyboardState().noteOn (1, note, 1.0f);
    else
        processor.getKeyboardState().noteOff (1, note, 0.0f);
}

void DeckPage::refreshRecord()
{
    const auto record = processor.getDeckRecord();
    auto text = record != nullptr ? record->name + "  (" + juce::String (record->seconds(), 1) + " s)" : juce::String ("NO RECORD");
    if (processor.getDeckRecordProblem().isNotEmpty())
        text += "   " + processor.getDeckRecordProblem() + ": playing the house record";
    if (text != shownRecord)
    {
        shownRecord = text;
        recordLabel.setText (text, juce::dontSendNotification);
        recordLabel.setColour (juce::Label::textColourId, processor.getDeckRecordProblem().isNotEmpty() ? ParagonColours::meterRed : ParagonColours::text);
    }
}

void DeckPage::timerCallback()
{
    refreshRecord();
    const auto on = processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() > 0.5f;
    for (auto& k : keys)
        k.button->setEnabled (on);   // off, the keys would play the synth

    const auto speed = processor.getDeck().getSpeed();
    juce::String speedText;
    if (! on)
        speedText = "DECK OFF";
    else if (std::abs (speed) < 0.0005f)
        speedText = "STOPPED";
    else
        speedText = juce::String (std::abs (speed) * 100.0f, 1) + "%" + (speed < 0.0f ? "  BACKWARDS" : "");
    speedLabel.setText (speedText, juce::dontSendNotification);

    const auto first = processor.getDeckSettings().firstKey;
    juce::String legend ("KEYS:  ");
    for (const auto& k : keys)
        legend << noteName (first + (int) k.key) << " " << deck::keyNames[(int) k.key] << "    ";
    legend << "PITCH WHEEL: THE HAND";
    keysLabel.setText (legend, juce::dontSendNotification);
}

std::vector<ParamControl*> DeckPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &top, &motor, &needle })
        for (auto& c : *list)
            result.push_back (c.get());
    return result;
}

void DeckPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 18.0f, 120.0f, 24.0f)));
    g.drawText ("THE DECK", juce::Rectangle<int> (24, 14, 160, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ("A turntable: scratch, hold, spin back, drop", juce::Rectangle<int> (24, 38, 260, 16), juce::Justification::centredLeft, false);
}

void DeckPage::resized()
{
    auto area = getLocalBounds().reduced (14, 10);
    auto bar = area.removeFromTop (54);
    bar.removeFromLeft (280);
    top[0]->setBounds (bar.removeFromLeft (70));
    bar.removeFromLeft (8);
    top[1]->setBounds (bar.removeFromLeft (110));
    bar.removeFromLeft (20);
    loadButton.setBounds (bar.removeFromLeft (150).withSizeKeepingCentre (150, 30));
    bar.removeFromLeft (8);
    houseButton.setBounds (bar.removeFromLeft (140).withSizeKeepingCentre (140, 30));
    bar.removeFromLeft (16);
    recordLabel.setBounds (bar);
    area.removeFromTop (8);

    auto left = area.removeFromLeft (juce::jmin (540, area.getHeight()));
    speedLabel.setBounds (left.removeFromBottom (30));
    platter.setBounds (left.withSizeKeepingCentre (juce::jmin (left.getWidth(), left.getHeight()), juce::jmin (left.getWidth(), left.getHeight())));
    area.removeFromLeft (16);

    strip.setBounds (area.removeFromTop (190));
    area.removeFromTop (12);
    auto keyRow = area.removeFromTop (44);
    const auto keyWidth = (keyRow.getWidth() - 7 * 6) / 8;
    for (auto& k : keys)
    {
        k.button->setBounds (keyRow.removeFromLeft (keyWidth));
        keyRow.removeFromLeft (6);
    }
    area.removeFromTop (6);
    keysLabel.setBounds (area.removeFromTop (22));
    area.removeFromTop (14);
    std::vector<juce::Component*> row;
    for (auto& c : motor)
        row.push_back (c.get());
    SynthPanel::layoutRow (area.removeFromTop (ParamControl::knobHeight), row, 110);
    area.removeFromTop (14);
    row.clear();
    for (auto& c : needle)
        row.push_back (c.get());
    SynthPanel::layoutRow (area.removeFromTop (ParamControl::knobHeight), row, 110);
}
