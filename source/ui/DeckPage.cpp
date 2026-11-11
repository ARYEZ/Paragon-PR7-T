#include "DeckPage.h"

#include "ui/DockPanels.h"
#include "ui/ParagonLookAndFeel.h"

#include "DeckImages.h"

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
namespace
{
    // THE TURNTABLE's pictures (0.3.0, resources/deck/: rendered by turntable3d.py, made ready by process_deck.py)
    // and where things are in them, in the picture's pixels (1400 x 1080, the plugin's 2x).
    struct DeckPictures
    {
        juce::Image deck, lit, platter, arm;
        float width = 1400.0f, height = 1080.0f;
        juce::Point<float> platterCentre, pivot, stylus;
        float platterRadius = 0.0f, recordRadius = 0.0f, armLength = 0.0f, grooveStart = 0.0f, grooveEnd = 0.0f;
        juce::Rectangle<float> platterAt, armAt;   // where the turning layers were cut from
        float branch = 1.0f;                       // which way round the arm meets the groove (see needleAt)

        // Where the stylus is when the needle is at radius `r` round the platter's centre: where that circle meets
        // the arm's reach from its pivot (turntable3d.py's needle_at).
        juce::Point<float> needleAt (float r, float side) const
        {
            const auto d = pivot.getDistanceFrom (platterCentre);
            const auto a = (d * d - armLength * armLength + r * r) / (2.0f * d);
            const auto h = std::sqrt (juce::jmax (0.0f, r * r - a * a));
            const auto u = (pivot - platterCentre) / d;
            return platterCentre + u * a + juce::Point<float> (-u.y, u.x) * (h * side);
        }
    };

    // JPEG moves a few colours a few degrees (and rings beside bright glints): every hue pulled into the family's
    // 318-42 degrees once more as the pictures load, as PR7-D's kit view, PR7-I's guitar and PR7-O's strings do.
    juce::Image inFamilyHues (juce::Image image)
    {
        image = image.convertedToFormat (juce::Image::ARGB);
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto c = pixels.getPixelColour (x, y);
                const auto hue = c.getHue() * 360.0f;
                if (c.getSaturation() > 0.0f && hue > 41.0f && hue < 319.0f)
                    pixels.setPixelColour (x, y, c.withHue ((hue < 180.0f ? 41.0f : 319.0f) / 360.0f));
            }
        return image;
    }

    const DeckPictures& deckPictures()
    {
        static const DeckPictures pictures = []
        {
            DeckPictures p;
            p.deck = inFamilyHues (juce::ImageFileFormat::loadFrom (DeckImages::deck_jpg, DeckImages::deck_jpgSize));
            p.lit = inFamilyHues (juce::ImageFileFormat::loadFrom (DeckImages::decklit_jpg, DeckImages::decklit_jpgSize));
            p.platter = juce::ImageCache::getFromMemory (DeckImages::platter_png, DeckImages::platter_pngSize);
            p.arm = juce::ImageCache::getFromMemory (DeckImages::arm_png, DeckImages::arm_pngSize);
            const auto json = juce::JSON::parse (juce::String::fromUTF8 (DeckImages::deck_json, DeckImages::deck_jsonSize));
            const auto point = [&json] (const char* key) { const auto v = json[key]; return juce::Point<float> ((float) v[0], (float) v[1]); };
            const auto rect = [&json] (const char* key)
            {
                const auto v = json["layers"][key];
                return juce::Rectangle<float> ((float) v[0], (float) v[1], (float) v[2], (float) v[3]);
            };
            p.width = (float) json["width"];
            p.height = (float) json["height"];
            p.platterCentre = point ("platterCentre");
            p.pivot = point ("pivot");
            p.stylus = point ("stylus");
            p.platterRadius = (float) json["platterRadius"];
            p.recordRadius = (float) json["recordRadius"];
            p.armLength = (float) json["arm"];
            p.grooveStart = (float) json["grooveStart"];
            p.grooveEnd = (float) json["grooveEnd"];
            p.platterAt = rect ("platter");
            p.armAt = rect ("arm");
            // The render's y runs down, its world's up: find which meeting is the one the arm was rendered at.
            p.branch = p.needleAt (p.grooveStart, 1.0f).getDistanceFrom (p.stylus) <= p.needleAt (p.grooveStart, -1.0f).getDistanceFrom (p.stylus) ? 1.0f : -1.0f;
            return p;
        }();
        return pictures;
    }
}

DeckPlatter::DeckPlatter (ParagonX1AudioProcessor& p) : processor (p)
{
    setTooltip ("THE TURNTABLE. Grab the record and drag round it to scratch: clockwise moves it forwards, back moves it "
                "back, holding still stops it. Let go and the motor takes it again. The label's mark turns with the "
                "record, the arm moves in as the needle goes through it, and the neon brightens as it turns.");
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    deckPictures();
    startTimerHz (30);
}

std::vector<juce::Image> DeckPlatter::picturesForTest()
{
    const auto& p = deckPictures();
    return { p.deck, p.lit, p.platter, p.arm };
}

juce::Rectangle<float> DeckPlatter::pictureArea() const
{
    const auto& pics = deckPictures();
    return juce::RectanglePlacement (juce::RectanglePlacement::centred)
        .appliedTo (juce::Rectangle<float> (pics.width, pics.height), getLocalBounds().toFloat());
}

juce::Point<float> DeckPlatter::getPlatterCentre() const
{
    const auto area = pictureArea();
    const auto k = area.getWidth() / deckPictures().width;
    return area.getTopLeft() + deckPictures().platterCentre * k;
}

float DeckPlatter::getRecordRadius() const
{
    return deckPictures().recordRadius * pictureArea().getWidth() / deckPictures().width;
}

void DeckPlatter::timerCallback()
{
    const auto& deck = processor.getDeck();
    const auto turns = deck.getPositionSeconds() / deck::secondsPerTurn;
    const auto on = processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() > 0.5f;
    const auto lit = on ? juce::jlimit (0.0f, 1.0f, std::abs (deck.getSpeed())) : 0.0f;
    const auto record = processor.getDeckRecord();
    const auto through = record != nullptr && record->seconds() > 0.0
                       ? (float) juce::jlimit (0.0, 1.0, deck.getPositionSeconds() / record->seconds()) : 0.0f;
    const auto& pics = deckPictures();
    const auto stylus = pics.needleAt (pics.grooveStart + (pics.grooveEnd - pics.grooveStart) * through, pics.branch);
    const auto angleOf = [] (juce::Point<float> v) { return std::atan2 (v.y, v.x); };   // (y down: clockwise positive)
    const auto swing = angleOf (stylus - pics.pivot) - angleOf (pics.stylus - pics.pivot);
    // Repainted only when something shows it (a stopped record costs nothing).
    if (std::abs (turns - shownTurns) * twoPi * pics.recordRadius > 0.25 || std::abs (lit - shownLit) > 0.01f || std::abs (swing - armAngle) > 0.0005f)
    {
        shownTurns = turns;
        shownLit = lit;
        armAngle = swing;
        repaint();
    }
}

float DeckPlatter::angleAt (juce::Point<float> point) const
{
    const auto centre = getPlatterCentre();
    return std::atan2 (point.x - centre.x, centre.y - point.y);   // clockwise from twelve o'clock
}

void DeckPlatter::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.getDistanceFrom (getPlatterCentre()) > getRecordRadius() * 1.1f)
        return;   // a hand on the record, not beside it
    grabbed = true;
    handTurns = 0.0f;
    lastAngle = angleAt (e.position);
    processor.setDeckMouseHand (true, 0.0f);
    repaint();
}

void DeckPlatter::mouseDrag (const juce::MouseEvent& e)
{
    if (! grabbed)
        return;
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
    if (! grabbed)
        return;
    grabbed = false;
    processor.setDeckMouseHand (false, 0.0f);
    repaint();
}

void DeckPlatter::prepare (float scale)
{
    const auto area = pictureArea();
    if (juce::exactlyEqual (shown.scale, scale) && shown.area == area && shown.deck.isValid())
        return;
    const auto& pics = deckPictures();
    shown.scale = scale;
    shown.area = area;
    const auto k = area.getWidth() / pics.width * scale;   // the picture's pixels to the screen's
    const auto shrink = [k] (const juce::Image& image)
    {
        return image.isValid() ? image.rescaled (juce::jmax (1, juce::roundToInt ((float) image.getWidth() * k)),
                                                 juce::jmax (1, juce::roundToInt ((float) image.getHeight() * k)),
                                                 juce::Graphics::highResamplingQuality)
                               : juce::Image();
    };
    shown.deck = shrink (pics.deck);
    shown.lit = shrink (pics.lit);
    shown.platter = shrink (pics.platter);
    shown.arm = shrink (pics.arm);
}

void DeckPlatter::paint (juce::Graphics& g)
{
    const auto scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
    prepare (scale);
    if (! shown.deck.isValid())
        return;
    const auto& pics = deckPictures();
    const auto area = shown.area;
    const auto k = (float) shown.deck.getWidth() / pics.width;   // the picture's pixels to the screen's (as shrunk)

    // The deck copied, not resampled: the context's scale cancels ours and its corner sits on a whole screen pixel.
    const auto origin = (area.getTopLeft() * scale).roundToInt().toFloat();
    const auto onScreen = [&] (juce::Point<float> at) { return juce::AffineTransform::translation (origin.x + at.x, origin.y + at.y).scaled (1.0f / scale); };
    g.drawImageTransformed (shown.deck, onScreen ({}));
    if (shownLit > 0.01f && shown.lit.isValid())
    {
        g.setOpacity (shownLit);
        g.drawImageTransformed (shown.lit, onScreen ({}));
        g.setOpacity (1.0f);
    }

    // The platter, turned round its centre as far as the record has turned.
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    const auto rotation = (float) (std::fmod (shownTurns, 1.0) * twoPi);
    const auto platterCorner = pics.platterAt.getTopLeft() * k;
    const auto platterCentre = pics.platterCentre * k - platterCorner;   // in the platter's own picture
    g.drawImageTransformed (shown.platter, juce::AffineTransform::translation (-platterCentre.x, -platterCentre.y)
                                               .rotated (rotation)
                                               .translated (origin.x + platterCorner.x + platterCentre.x, origin.y + platterCorner.y + platterCentre.y)
                                               .scaled (1.0f / scale));

    // A still sheen over the turning vinyl (the light stays where it is as the record goes round under it).
    const auto centre = getPlatterCentre();
    const auto radius = getRecordRadius();
    {
        juce::Path sheen;
        const auto disc = juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (centre);
        sheen.addPieSegment (disc, -0.9f, -0.35f, 0.4f);
        sheen.addPieSegment (disc, juce::MathConstants<float>::pi - 0.9f, juce::MathConstants<float>::pi - 0.35f, 0.4f);
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillPath (sheen);
    }

    // The arm, swung about its pivot as far as the needle has gone through the record. (Opaque again: an image is
    // drawn at the current colour's opacity, and the sheen's was 4.5%.)
    g.setOpacity (1.0f);
    const auto armCorner = pics.armAt.getTopLeft() * k;
    const auto pivot = pics.pivot * k - armCorner;
    g.drawImageTransformed (shown.arm, juce::AffineTransform::translation (-pivot.x, -pivot.y)
                                           .rotated (armAngle)
                                           .translated (origin.x + armCorner.x + pivot.x, origin.y + armCorner.y + pivot.y)
                                           .scaled (1.0f / scale));

    // A hand on the record: its edge lit.
    if (grabbed)
    {
        g.setColour (ParagonColours::accent.withAlpha (0.85f));
        g.drawEllipse (juce::Rectangle<float> (2.0f * radius, 2.0f * radius).withCentre (centre), 2.5f);
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

    // THE TURNTABLE at its picture's shape, as big as the height allows (about half the page).
    const auto turntableHeight = area.getHeight() - 30;
    auto left = area.removeFromLeft (juce::jmin (area.getWidth() / 2, juce::roundToInt ((float) turntableHeight * DeckPlatter::pictureAspect)));
    speedLabel.setBounds (left.removeFromBottom (30));
    platter.setBounds (left);
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
    SynthPanel::layoutRow (area.removeFromTop (ParamControl::knobHeight), row, 100);
    area.removeFromTop (14);
    row.clear();
    for (auto& c : needle)
        row.push_back (c.get());
    SynthPanel::layoutRow (area.removeFromTop (ParamControl::knobHeight), row, 100);
}
