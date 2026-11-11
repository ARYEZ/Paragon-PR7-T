#include "DeckPage.h"

#include "ui/DockPanels.h"
#include "ui/Finish.h"
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
DeckCrossfader::DeckCrossfader (ParagonX1AudioProcessor& p)
    : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      processor (p),
      attachment (*p.getParameters().getParameter (ParamIDs::deckFader), *this)
{
    setTooltip ("THE CROSSFADER: drag it to cut the deck in and out (the DECK end open, the other end cut); double-click "
                "to open it. CUT and TRANSFORM flip it from the keys; HAMSTER (FADER page) turns it round; CURVE sets "
                "how sharply it cuts. Right-click a knob on the rack to MIDI-learn a controller's fader.");
    setDoubleClickReturnValue (true, 1.0);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    startTimerHz (30);
}

void DeckCrossfader::timerCallback()
{
    const auto gain = processor.getDeck().getFaderGain();
    const auto hamster = processor.getParameters().getRawParameterValue (ParamIDs::deckHamster)->load() > 0.5f;
    if (std::abs (gain - shownGain) > 0.01f || hamster != shownHamster)
    {
        shownGain = gain;
        shownHamster = hamster;
        repaint();
    }
}

void DeckCrossfader::paint (juce::Graphics& g)
{
    // A mixer's fader plate: the slot, its marks, DECK printed at the open end and CUT at the other, the cap (black,
    // a neon line across it), and the slot lit on the deck's side as much as the deck is heard.
    auto plate = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff0b090c));
    g.fillRoundedRectangle (plate, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (plate.reduced (0.5f), 6.0f, 1.0f);
    auto inner = plate.reduced (16.0f, 8.0f);
    auto labels = inner.removeFromBottom (14.0f);
    const auto slot = inner.withSizeKeepingCentre (inner.getWidth() - 40.0f, 6.0f);
    const auto at = (float) valueToProportionOfLength (getValue());
    const auto capX = slot.getX() + at * slot.getWidth();

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int k = 0; k <= 10; ++k)
    {
        const auto x = slot.getX() + slot.getWidth() * (float) k / 10.0f;
        g.setColour (ParagonColours::textDim.withAlpha (k % 5 == 0 ? 0.7f : 0.35f));
        g.fillRect (juce::Rectangle<float> (x - 0.5f, slot.getY() - (k % 5 == 0 ? 12.0f : 8.0f), 1.0f, k % 5 == 0 ? 6.0f : 4.0f));
    }
    g.setColour (ParagonColours::textDim);
    g.drawText (shownHamster ? "DECK" : "CUT", labels, juce::Justification::centredLeft, false);
    g.drawText (shownHamster ? "CUT" : "DECK", labels, juce::Justification::centredRight, false);
    g.drawText ("CROSSFADER", labels, juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff020102));
    g.fillRoundedRectangle (slot, 3.0f);
    // The travel that's open lit, from the CUT end to the cap (all of it with the fader open), brighter the more the
    // deck is heard (a CUT or TRANSFORM dims it as they cut).
    const auto lit = shownHamster ? slot.withLeft (capX) : slot.withRight (capX);
    g.setColour (ParagonColours::glow.withAlpha (0.25f + 0.75f * shownGain));
    g.fillRoundedRectangle (lit, 3.0f);
    if (shownGain > 0.01f)
    {
        g.setColour (ParagonColours::glow.withAlpha (0.25f * shownGain));
        g.fillRoundedRectangle (lit.expanded (0.0f, 4.0f), 5.0f);
    }

    const auto cap = juce::Rectangle<float> (26.0f, juce::jmin (inner.getHeight(), 34.0f)).withCentre ({ capX, slot.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (2.0f, 3.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c282e), cap.getX(), cap.getY(), juce::Colour (0xff0d0b0e), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (ParagonColours::panelEdge.brighter (0.4f));
    g.drawRoundedRectangle (cap.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (ParagonColours::accent);
    g.fillRect (juce::Rectangle<float> (capX - 1.0f, cap.getY() + 4.0f, 2.0f, cap.getHeight() - 8.0f));
}

//==============================================================================
DeckPage::DeckPage (ParagonX1AudioProcessor& p)
    : processor (p), platter (p), strip (p), crossfader (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip, bool knob = false)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip, knob));
    };
    // DECK: the turntable.
    make (deckControls, ParamIDs::deckPitch, "PITCH", "The pitch fader: the record's speed, and with it its pitch, up to 16% either way.");
    make (deckControls, ParamIDs::deckSpeed, "RPM", "33 or 45: 45 plays the record 1.35 times as fast (about 5 semitones up).", true);
    make (deckControls, ParamIDs::deckStart, "START", "How long the motor takes to bring a stopped record up to speed (PLAY).");
    make (deckControls, ParamIDs::deckBrake, "BRAKE", "How long the record takes to stop (STOP). Long: a slow tape-stop wind-down.");
    make (deckControls, ParamIDs::deckSpin, "SPIN", "BACKSPIN: how fast the record is thrown back, times its normal speed.");
    make (deckControls, ParamIDs::deckSpinTime, "SPIN TIME", "BACKSPIN: how long it takes to slow to a stop.");
    make (deckControls, ParamIDs::deckReach, "REACH", "How far the pitch wheel moves the record, all the way up or down, in turns of "
                                                      "the platter (a turn: 1.8 seconds of the record).");
    make (deckControls, ParamIDs::deckNeedle, "NEEDLE", "How loud the needle's landing is on a DROP: its thump and crackle "
                                                        "(louder for a harder key).");
    // FADER: the crossfader.
    make (faderControls, ParamIDs::deckFader, "FADER", "THE CROSSFADER: how open the deck's channel is (OPEN: heard; CUT: silent). "
                                                       "The fader under the strip is the same; a controller's fader can be MIDI-learned here.");
    make (faderControls, ParamIDs::deckCurve, "CURVE", "How the fader cuts: SMOOTH, a mixing fade (-3 dB at the middle); CUT, a "
                                                       "scratch mixer's, fully open after 1% of its travel.");
    make (faderControls, ParamIDs::deckTransform, "TRANSFORM", "TRANSFORM's rate: held, the key chops the deck on and off this fast, "
                                                               "in time with the song (1/16: four times a beat).", true);
    make (faderControls, ParamIDs::deckHamster, "HAMSTER", "Turns the fader round, as many scratch DJs set theirs: the deck's end "
                                                           "moves to the other side.");
    // CUES: where the drops land, and the keys.
    for (int cue = 0; cue < 4; ++cue)
        make (cueControls, ParamIDs::deckCues[cue], "CUE " + juce::String (cue + 1),
              "Where DROP " + juce::String (cue + 1) + " puts the needle, from the record's start (0%) to its end (100%).");
    keysMenu = std::make_unique<ParamControl> (state, ParamIDs::deckKeys, "KEYS", "The octave the deck's 12 keys start on (C1: the "
                                                                                 "on-screen keyboard's lowest).");
    keysMenu->setCompact (true);
    // SCRATCH (0.5.0): the scratch library.
    make (scratchControls, ParamIDs::deckScratchReach, "REACH", "The scratch library's stroke at full velocity, in turns of the platter "
                                                                "(a turn: 1.8 seconds of the record). A softer key: down to 40% of it.");
    make (scratchControls, ParamIDs::deckScratchRate, "RATE", "The techniques at half, normal or double speed (each takes a set number "
                                                              "of beats at the song's tempo: BABY half a beat, TRANSFORMER two).", true);
    make (scratchControls, ParamIDs::deckModHand, "MOD HAND", "The mod wheel as a hand on the record (it stays where you leave it, unlike "
                                                              "the pitch wheel's spring). On, the mod wheel is the deck's, not the synth's.");
    scratchKeysMenu = std::make_unique<ParamControl> (state, ParamIDs::deckScratchKeys, "SCRATCH KEYS",
                                                      "The octave the scratch library's 12 keys start on (a technique a key), or OFF.");
    scratchKeysMenu->setCompact (true);
    level = std::make_unique<ParamControl> (state, ParamIDs::deckLevel, "LEVEL", "The deck's level, before the rack (EQ, DOMINION...), "
                                                                                 "OUTPUT and the LIMITER, which it goes through as the synth does.");

    // On the rack, in its pearl black (lettered in its ink whatever the window's finish).
    Finish::paintAs (Finish::Type::pearlBlack);
    for (auto* list : { &deckControls, &faderControls, &cueControls, &scratchControls })
        for (auto& c : *list)
        {
            c->setRack (true);
            rack.addChildComponent (*c);
        }
    scratchKeysMenu->setRack (true);
    rack.addChildComponent (*scratchKeysMenu);
    for (auto* c : { keysMenu.get(), level.get() })
    {
        c->setRack (true);
        rack.addAndMakeVisible (*c);
    }
    Finish::paintAs (std::nullopt);

    // The rack's keys: ON (lit while the deck is on), LOAD, HOUSE.
    for (auto* key : { &onKey, &loadKey, &houseKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        rack.addAndMakeVisible (*key);
    }
    onKey.setComponentID ("deckOn");
    onKey.setClickingTogglesState (true);
    onKey.setTooltip ("THE DECK on or off. On, its 12 keys (CUES > KEYS) and the pitch wheel play the deck, not the synth; "
                      "off, the synth has them all, as on PR7-K.");
    onAttachment = std::make_unique<juce::ButtonParameterAttachment> (*state.getParameter (ParamIDs::deckOn), onKey);
    loadKey.setComponentID ("deckLoad");
    loadKey.setTooltip ("Puts a record on the deck: a sample from a file (WAV, AIFF, FLAC or Ogg, up to 3 minutes). "
                        "The project remembers it.");
    houseKey.setComponentID ("deckHouse");
    houseKey.setTooltip ("Puts THE HOUSE RECORD on the deck, our own built-in scratch record: an \"ahh\", a horn stab, "
                         "a drum break and a 1 kHz tone, two seconds apart, at the cues' first places.");
    loadKey.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Put a record on the deck", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& c)
                              {
                                  if (c.getResult().existsAsFile())
                                      processor.loadDeckRecord (c.getResult());
                                  timerCallback();
                              });
    };
    houseKey.onClick = [this] { processor.useHouseRecord(); timerCallback(); };
    rack.onPageChanged = [this] { showRackPage(); };
    // A technique clicked on the SCRATCH screen: its key pressed, as long as the mouse is down (as from MIDI).
    rack.onScratchPressed = [this] (int technique, bool down)
    {
        const auto first = processor.getDeckSettings().firstScratchKey;
        if (first < 0 || processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() <= 0.5f)
            return;   // OFF, or the deck off: the key would play the synth
        if (down)
            processor.getKeyboardState().noteOn (1, first + technique, 0.9f);
        else
            processor.getKeyboardState().noteOff (1, first + technique, 0.0f);
    };
    addAndMakeVisible (rack);
    showRackPage();

    // The transport keys under the strip: the same as the deck's 12 keys on a keyboard.
    const std::pair<deck::Key, const char*> shown[] = {
        { deck::Key::play, "Starts the motor: the record comes up to speed over START." },
        { deck::Key::stop, "Stops the motor: the record winds down over BRAKE." },
        { deck::Key::backspin, "Throws the record back (SPIN), slowing to a stop over SPIN TIME. PLAY or a DROP starts it again." },
        { deck::Key::hold, "A hand holding the record still while it's held down; let go and the record catches up with the "
                           "platter at once (stabs: hold, DROP, let go)." },
        { deck::Key::drop1, "Drops the needle at CUE 1, the motor on." },
        { deck::Key::drop2, "Drops the needle at CUE 2, the motor on." },
        { deck::Key::drop3, "Drops the needle at CUE 3, the motor on." },
        { deck::Key::drop4, "Drops the needle at CUE 4, the motor on." },
        { deck::Key::cut, "THE CROSSFADER flipped while it's held: open to cut, or (the fader cut) a tap that opens it. With "
                          "the hand: chirps, stabs." },
        { deck::Key::transform, "THE CROSSFADER chopped on and off in time with the song while it's held, at TRANSFORM's rate "
                                "(FADER page): a transformer scratch." } };
    for (const auto& [key, tip] : shown)
    {
        auto button = std::make_unique<juce::TextButton> (deck::keyNames[(int) key]);
        styleButton (*button);
        button->setTooltip (tip);
        if (key == deck::Key::hold || key == deck::Key::cut || key == deck::Key::transform)
        {
            // Held keys: down while the button's down.
            auto* b = button.get();
            b->onStateChange = [this, b, key = key, down = false] () mutable
            {
                if (b->isDown() != down)
                {
                    down = b->isDown();
                    press (key, down);
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

    styleButton (rackButton);
    rackButton.setComponentID ("deckRackButton");
    rackButton.setTooltip ("Hides the rack (the turntable gets the page's height) or shows it again.");
    rackButton.onClick = [this] { setRackShown (! rackShown); };
    addAndMakeVisible (rackButton);

    for (auto* label : { &speedLabel, &keysLabel })
    {
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (*label);
    }
    speedLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    speedLabel.setJustificationType (juce::Justification::centred);
    speedLabel.setColour (juce::Label::textColourId, ParagonColours::accent);
    keysLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));

    addAndMakeVisible (platter);
    addAndMakeVisible (strip);
    addAndMakeVisible (crossfader);
    setRackShown (true);
    timerCallback();
    startTimerHz (20);
}

DeckPage::~DeckPage()
{
    processor.setDeckMouseHand (false, 0.0f);
}

void DeckPage::setRackShown (bool shown)
{
    rackShown = shown;
    rack.setVisible (shown);
    rackButton.setButtonText (shown ? "HIDE RACK" : "SHOW RACK");
    resized();
    repaint();
}

void DeckPage::showRackPage()
{
    const auto page = rack.getPage();
    for (auto& c : deckControls)
        c->setVisible (page == DeckRack::Page::deck);
    for (auto& c : faderControls)
        c->setVisible (page == DeckRack::Page::fader);
    for (auto& c : cueControls)
        c->setVisible (page == DeckRack::Page::cues);
    keysMenu->setVisible (page == DeckRack::Page::cues);
    for (auto& c : scratchControls)
        c->setVisible (page == DeckRack::Page::scratch);
    scratchKeysMenu->setVisible (page == DeckRack::Page::scratch);
}

void DeckPage::press (deck::Key key, bool down)
{
    const auto note = processor.getDeckSettings().firstKey + (int) key;
    if (down)
        processor.getKeyboardState().noteOn (1, note, 1.0f);
    else
        processor.getKeyboardState().noteOff (1, note, 0.0f);
}

void DeckPage::timerCallback()
{
    const auto on = processor.getParameters().getRawParameterValue (ParamIDs::deckOn)->load() > 0.5f;
    for (auto& k : keys)
        k.button->setEnabled (on);   // off, the keys would play the synth

    const auto& deck = processor.getDeck();
    const auto speed = deck.getSpeed();
    juce::String state;
    if (! on)
        state = "DECK OFF";
    else if (std::abs (speed) < 0.0005f)
        state = "STOPPED";
    else
        state = juce::String (std::abs (speed) * 100.0f, 1) + "%" + (speed < 0.0f ? "  BACKWARDS" : "");
    speedLabel.setText (state, juce::dontSendNotification);

    // The rack's screen: the record, how it's turning, where the needle is.
    const auto settings = processor.getDeckSettings();
    const auto record = processor.getDeckRecord();
    const auto problem = processor.getDeckRecordProblem();
    DeckRack::Screen screen;
    screen.caption = processor.getDeckRecordFile().isEmpty() ? "THE RECORD: BUILT IN" : "THE RECORD";
    screen.record = record != nullptr ? record->name : juce::String ("NO RECORD");
    screen.state = ! on ? juce::String ("OFF") : std::abs (speed) < 0.0005f ? juce::String ("STOPPED") : speed < 0.0f ? juce::String ("BACKWARDS") : juce::String ("PLAYING");
    screen.line1 = record != nullptr ? juce::String (deck.getPositionSeconds(), 1) + " / " + juce::String (record->seconds(), 1) + " S" : juce::String();
    screen.problem = problem.isNotEmpty();
    screen.line2 = screen.problem ? problem + ": THE HOUSE RECORD MEANWHILE"
                                  : "KEYS FROM " + noteName (settings.firstKey) + "  -  PITCH WHEEL: THE HAND";
    rack.setScreen (screen);
    rack.setOn (on);
    const auto peak = processor.takeDeckPeak();
    rack.setMeterDb (peak > 1.0e-5f ? juce::Decibels::gainToDecibels (peak) : -100.0f);
    rack.setFaderGain (deck.getFaderGain());
    rack.setFaderInfo ({ "CUT " + noteName (settings.firstKey + (int) deck::Key::cut) + "    TRANSFORM " + noteName (settings.firstKey + (int) deck::Key::transform),
                         "CUT FLIPS IT WHILE HELD; TRANSFORM CHOPS IT AT " + deck::transformNames[settings.transformRate],
                         settings.hamster ? juce::String ("HAMSTER: TURNED ROUND") : juce::String ("THE DECK AT THE FADER'S RIGHT") });
    juce::StringArray keyMap;
    for (int k = 0; k < deck::numKeys; ++k)
        if (deck::keyNames[k].isNotEmpty())
            keyMap.add (noteName (settings.firstKey + k) + "  " + deck::keyNames[k]);
    rack.setKeyMap (keyMap);
    juce::StringArray scratches;
    for (int t = 0; t < deck::numScratches; ++t)
        scratches.add ((settings.firstScratchKey >= 0 ? noteName (settings.firstScratchKey + t) : juce::String ("--")) + "  " + deck::scratchNames[t]);
    rack.setScratchList (scratches, deck.getScratch());

    juce::String legend ("KEYS:  ");
    for (const auto& k : keys)
        legend << noteName (settings.firstKey + (int) k.key) << " " << deck::keyNames[(int) k.key] << "   ";
    legend << "PITCH WHEEL: THE HAND";
    keysLabel.setText (legend, juce::dontSendNotification);
    if (! rackShown)
        repaint (0, 0, getWidth(), 44);   // the header names the record while the rack's hidden
}

std::vector<ParamControl*> DeckPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto* list : { &deckControls, &faderControls, &cueControls, &scratchControls })
        for (auto& c : *list)
            result.push_back (c.get());
    result.push_back (keysMenu.get());
    result.push_back (scratchKeysMenu.get());
    result.push_back (level.get());
    return result;
}

void DeckPage::paint (juce::Graphics& g)
{
    DockStyle::drawPanel (g, getLocalBounds().toFloat());
    if (rackShown)
        return;
    // The rack hidden: a header naming the deck and its record.
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setGradientFill (ParagonLookAndFeel::menuHighlight (juce::Rectangle<float> (24.0f, 16.0f, 120.0f, 24.0f)));
    g.drawText ("THE DECK", juce::Rectangle<int> (24, 12, 120, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const auto record = processor.getDeckRecord();
    g.drawText (record != nullptr ? record->name + "  (" + juce::String (record->seconds(), 1) + " s)" : juce::String(),
                juce::Rectangle<int> (150, 12, 500, 26), juce::Justification::centredLeft, false);
}

void DeckPage::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    if (rackShown)
    {
        rack.setBounds (area.removeFromTop (DeckRack::preferredHeight));
        area.removeFromTop (4);
        // The rack's controls in the places it gives.
        for (int i = 0; i < (int) deckControls.size(); ++i)
            deckControls[(size_t) i]->setBounds (rack.knobCell (i));
        for (int i = 0; i < (int) faderControls.size(); ++i)
            faderControls[(size_t) i]->setBounds (rack.knobCell (i));
        for (int i = 0; i < (int) cueControls.size(); ++i)
            cueControls[(size_t) i]->setBounds (rack.knobCell (i));
        for (int i = 0; i < (int) scratchControls.size(); ++i)
            scratchControls[(size_t) i]->setBounds (rack.knobCell (i));
        keysMenu->setBounds (rack.menuCell().reduced (6, 2));
        scratchKeysMenu->setBounds (rack.menuCell().reduced (6, 2));
        level->setBounds (rack.sideKnobCell());
        onKey.setBounds (rack.keyCell (0));
        loadKey.setBounds (rack.keyCell (1));
        houseKey.setBounds (rack.keyCell (2));
    }
    else
        area.removeFromTop (36);   // the header (painted)

    // THE TURNTABLE at its picture's shape, as big as the height allows (at most half the page).
    const auto turntableHeight = area.getHeight() - 30;
    auto left = area.removeFromLeft (juce::jmin (area.getWidth() / 2, juce::roundToInt ((float) turntableHeight * DeckPlatter::pictureAspect)));
    speedLabel.setBounds (left.removeFromBottom (30));
    platter.setBounds (left);
    area.removeFromLeft (16);

    // Beside it: HIDE / SHOW RACK, the record's strip, the keys, the crossfader, the key legend.
    auto top = area.removeFromTop (28);
    rackButton.setBounds ((rackShown ? top : juce::Rectangle<int> (getWidth() - 150, 12, 130, 28)).removeFromRight (130));
    area.removeFromTop (4);
    // The ten keys in a row where they fit with their names whole (about 90 px each), else in two rows of five.
    const auto twoRows = (area.getWidth() - 9 * 5) / 10 < 90;
    const auto keysHeight = twoRows ? 2 * 40 + 6 : 44;
    const auto spare = area.getHeight() - (keysHeight + 10 + 22 + 10);
    strip.setBounds (area.removeFromTop (juce::jmax (90, spare * 55 / 100)));
    area.removeFromTop (10);
    auto keyArea = area.removeFromTop (keysHeight);
    const auto perRow = twoRows ? 5 : 10;
    for (size_t k = 0; k < keys.size(); ++k)
    {
        auto row = twoRows ? (k < 5 ? keyArea.withHeight (40) : keyArea.withTrimmedTop (46)) : keyArea;
        const auto width = (row.getWidth() - (perRow - 1) * 5) / perRow;
        const auto column = (int) k % perRow;
        keys[k].button->setBounds (row.getX() + column * (width + 5), row.getY(), width, row.getHeight());
    }
    area.removeFromTop (10);
    keysLabel.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (8);
    crossfader.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 80)));
}
