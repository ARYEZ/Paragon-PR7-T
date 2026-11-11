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

DeckPlatter::DeckPlatter (ParagonX1AudioProcessor& p, int d) : processor (p), deckIndex (d)
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
    const auto& deck = processor.getDeck (deckIndex);
    const auto turns = deck.getPositionSeconds() / deck::secondsPerTurn;
    const auto on = processor.getParameters().getRawParameterValue (ParamIDs::deckIds[deckIndex].on)->load() > 0.5f;
    const auto lit = on ? juce::jlimit (0.0f, 1.0f, std::abs (deck.getSpeed())) : 0.0f;
    const auto record = processor.getDeckRecord (deckIndex);
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
    processor.setDeckMouseHand (true, 0.0f, deckIndex);
    if (onGrab != nullptr)
        onGrab();
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
    processor.setDeckMouseHand (true, handTurns, deckIndex);
}

void DeckPlatter::mouseUp (const juce::MouseEvent&)
{
    if (! grabbed)
        return;
    grabbed = false;
    processor.setDeckMouseHand (false, 0.0f, deckIndex);
    repaint();
}

void DeckPlatter::setSelected (bool isSelected)
{
    if (selected != isSelected)
    {
        selected = isSelected;
        repaint();
    }
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

    // 0.9.0: the deck's name in its corner, lit while it's the one the rack shows.
    const auto nameArea = juce::Rectangle<float> (area.getX() + 10.0f, area.getY() + 8.0f, 90.0f, 22.0f);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    if (selected)
        g.setGradientFill (ParagonLookAndFeel::menuHighlight (nameArea));
    else
        g.setColour (ParagonColours::textDim);
    g.drawText ("DECK " + ParamIDs::deckNames[deckIndex], nameArea, juce::Justification::centredLeft, false);
}

//==============================================================================
DeckStrip::DeckStrip (ParagonX1AudioProcessor& p, int d) : processor (p), deckIndex (d)
{
    setTooltip ("The record: its sound from start to end, the needle's place on it (the line), and the cue points DROP 1-4 "
                "land on (CUE 1-4 below).");
    startTimerHz (30);
}

void DeckStrip::timerCallback()
{
    shownSeconds = processor.getDeck (deckIndex).getPositionSeconds();
    repaint();
}

void DeckStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070507));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (ParagonColours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    const auto record = processor.getDeckRecord (deckIndex);
    auto area = bounds.reduced (14.0f, 10.0f);
    auto labels = area.removeFromTop (18.0f);
    // 0.9.0: which deck's, and its record's name.
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (ParagonColours::accent);
    const auto nameArea = juce::Rectangle<float> (area.getX(), area.getBottom() - 6.0f, juce::jmin (260.0f, area.getWidth() * 0.6f), 16.0f);
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
    const auto settings = processor.getDeckSettings (deckIndex);
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

    // 0.9.0: whose record it is, under it.
    g.setColour (juce::Colour (0xff070507).withAlpha (0.8f));
    g.fillRect (nameArea.withWidth (juce::jmin (nameArea.getWidth(), 12.0f + 6.5f * (float) (record->name.length() + 3))));
    g.setColour (ParagonColours::accent);
    g.drawText (ParamIDs::deckNames[deckIndex] + "  " + record->name.toUpperCase(), nameArea, juce::Justification::centredLeft, true);

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
    setTooltip ("THE CROSSFADER, between DECK A (left) and DECK B (right): towards A cuts B, towards B cuts A; in the "
                "middle both are heard (with CURVE at CUT, both fully). Double-click: the middle. Each deck's CUT and "
                "TRANSFORM keys cut its own side; HAMSTER (FADER page) turns it round; CURVE sets how sharply it cuts. "
                "Right-click FADER on the rack to MIDI-learn a controller's fader.");
    setDoubleClickReturnValue (true, 0.5);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    startTimerHz (30);
}

void DeckCrossfader::timerCallback()
{
    const std::array<float, 2> gain { processor.getDeck (0).getFaderGain(), processor.getDeck (1).getFaderGain() };
    const auto hamster = processor.getParameters().getRawParameterValue (ParamIDs::deckHamster)->load() > 0.5f;
    if (std::abs (gain[0] - shownGain[0]) > 0.01f || std::abs (gain[1] - shownGain[1]) > 0.01f || hamster != shownHamster)
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
    // 0.9.0: the A/B fader: A at the left, B at the right (HAMSTER: turned round), each end's name lit as much as its
    // deck is heard (a CUT or TRANSFORM dims it as they cut).
    const auto leftDeck = shownHamster ? 1 : 0;
    const auto endLabel = [&] (int d, juce::Justification where)
    {
        g.setColour (ParagonColours::textDim.interpolatedWith (ParagonColours::glow, shownGain[(size_t) d]));
        g.drawText ("DECK " + ParamIDs::deckNames[d], labels, where, false);
    };
    endLabel (leftDeck, juce::Justification::centredLeft);
    endLabel (1 - leftDeck, juce::Justification::centredRight);
    g.setColour (ParagonColours::textDim);
    g.drawText ("CROSSFADER", labels, juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff020102));
    g.fillRoundedRectangle (slot, 3.0f);
    // Each side of the cap lit by the deck whose end it runs to.
    for (const auto side : { 0, 1 })
    {
        const auto d = side == 0 ? leftDeck : 1 - leftDeck;
        const auto lit = side == 0 ? slot.withRight (capX) : slot.withLeft (capX);
        g.setColour (ParagonColours::glow.withAlpha (0.15f + 0.6f * shownGain[(size_t) d]));
        g.fillRoundedRectangle (lit, 3.0f);
        if (shownGain[(size_t) d] > 0.01f)
        {
            g.setColour (ParagonColours::glow.withAlpha (0.2f * shownGain[(size_t) d]));
            g.fillRoundedRectangle (lit.expanded (0.0f, 4.0f), 5.0f);
        }
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
    : processor (p), platter (p, 0), platterB (p, 1), strip (p, 0), stripB (p, 1), crossfader (p)
{
    auto& state = p.getParameters();
    const auto make = [&state] (auto& list, const char* id, const juce::String& label, const juce::String& tip, bool knob = false)
    {
        list.push_back (std::make_unique<ParamControl> (state, id, label, tip, knob));
    };
    // Each deck's own (0.9.0: deck A's and DECK B's, the same controls on their own parameters).
    for (int d = 0; d < ParamIDs::numDecks; ++d)
    {
        const auto& ids = ParamIDs::deckIds[d];
        auto& set = sets[(size_t) d];
        const auto deckName = "DECK " + ParamIDs::deckNames[d] + ": ";
        // DECK: the turntable.
        make (set.deckControls, ids.pitch, "PITCH", deckName + "the pitch fader: the record's speed, and with it its pitch, up to 16% either way.");
        make (set.deckControls, ids.speed, "RPM", deckName + "33 or 45: 45 plays the record 1.35 times as fast (about 5 semitones up).", true);
        make (set.deckControls, ids.start, "START", deckName + "how long the motor takes to bring a stopped record up to speed (PLAY).");
        make (set.deckControls, ids.brake, "BRAKE", deckName + "how long the record takes to stop (STOP). Long: a slow tape-stop wind-down.");
        make (set.deckControls, ids.spin, "SPIN", deckName + "BACKSPIN: how fast the record is thrown back, times its normal speed.");
        make (set.deckControls, ids.spinTime, "SPIN TIME", deckName + "BACKSPIN: how long it takes to slow to a stop.");
        make (set.deckControls, ids.reach, "REACH", deckName + "how far the pitch wheel moves the record (while HAND is this deck), all the way "
                                                    "up or down, in turns of the platter (a turn: 1.8 seconds of the record).");
        make (set.deckControls, ids.needle, "NEEDLE", deckName + "how loud the needle's landing is on a DROP: its thump and crackle "
                                                      "(louder for a harder key).");
        // CUES: where the drops land, and the keys.
        for (int cue = 0; cue < 4; ++cue)
            make (set.cueControls, ids.cues[cue], "CUE " + juce::String (cue + 1),
                  deckName + "where DROP " + juce::String (cue + 1) + " puts the needle, from the record's start (0%) to its end (100%).");
        set.keysMenu = std::make_unique<ParamControl> (state, ids.keys, "KEYS", deckName + "the octave its 12 keys start on (deck A: C1, the "
                                                                                "on-screen keyboard's lowest; DECK B: C3).");
        set.keysMenu->setCompact (true);
        // SCRATCH (0.5.0): the scratch library.
        make (set.scratchControls, ids.scratchReach, "REACH", deckName + "the scratch library's stroke at full velocity, in turns of the platter "
                                                                         "(a turn: 1.8 seconds of the record). A softer key: down to 40% of it.");
        make (set.scratchControls, ids.scratchRate, "RATE", deckName + "the techniques at half, normal or double speed (each takes a set number "
                                                                       "of beats at the song's tempo: BABY half a beat, TRANSFORMER two).", true);
        set.scratchKeysMenu = std::make_unique<ParamControl> (state, ids.scratchKeys, "SCRATCH KEYS",
                                                              deckName + "the octave its scratch library's 12 keys start on (a technique a key), or OFF.");
        set.scratchKeysMenu->setCompact (true);
        // MIXER (0.6.0): the cartridge, the mixer channel, the record's wear.
        make (set.mixerControls, ids.cart, "CART", deckName + "the cartridge's voicing: CLEAN (flat), SCRATCH (a presence lift, a softer top, as "
                                                   "scratch cartridges are known for), WARM (rounder highs, a little low weight). Ours.", true);
        make (set.mixerControls, ids.eq[0], "LOW", deckName + "the mixer channel's low band (under 250 Hz): up to +6 dB, down, or OFF at the bottom.");
        make (set.mixerControls, ids.eq[1], "MID", deckName + "the mixer channel's middle band (250 Hz to 2.5 kHz).");
        make (set.mixerControls, ids.eq[2], "HIGH", deckName + "the mixer channel's high band (over 2.5 kHz).");
        make (set.mixerControls, ids.filter, "FILTER", deckName + "the DJ filter: left a low-pass sweeping down to 60 Hz, right a high-pass up to "
                                                       "8 kHz; the centre off.");
        make (set.mixerControls, ids.filterRes, "RES", deckName + "the FILTER's resonance: the peak it sings with at its cutoff.");
        make (set.mixerControls, ids.wear, "WEAR", deckName + "a worn record: surface hiss and crackle that move with it (louder as it goes "
                                                   "faster, silent held) and a warp's once-a-turn wobble. NEW: none.");
        // ROUTING (0.7.0): the deck into the FX.
        make (set.routingControls, ids.sendDelay, "TO DELAY", deckName + "how much of the deck (after its fader) goes into DELAY's send, while "
                                                              "DELAY is on (the FX page's SENDS). Cut the fader and the echoes ring on: an echo out.");
        make (set.routingControls, ids.sendReverb, "TO REVERB", deckName + "how much of the deck (after its fader) goes into REVERB's send, while "
                                                                "REVERB is on (the FX page's SENDS).");
        make (set.routingControls, ids.insert, "INSERT", deckName + "an insert bus for the deck (A-D: the FX page's INSERTS: WAH, DRIVE, GLITCH, "
                                                         "MOD, LO-FI), as an oscillator's; None: straight into the mix.", true);
        const char* killNames[] = { "KILL LOW", "KILL MID", "KILL HIGH" };
        for (int band = 0; band < 3; ++band)
            make (set.killControls, ids.kill[band], killNames[band], deckName + "kills the band at once: the isolator's cut, as on a battle "
                                                                                "mixer (a quick ramp, never a click).");
        set.level = std::make_unique<ParamControl> (state, ids.level, "LEVEL", deckName + "its level, before the rack (EQ, DOMINION...), "
                                                                                "OUTPUT and the LIMITER, which it goes through as the synth does.");
    }
    // The mixer's, shared by the two decks. FADER: the crossfader.
    make (faderControls, ParamIDs::deckFader, "FADER", "THE CROSSFADER, from DECK A (left) to DECK B (right): towards one cuts the other; in "
                                                       "the middle both are heard. The fader under the turntables is the same; a controller's "
                                                       "fader can be MIDI-learned here.");
    make (faderControls, ParamIDs::deckCurve, "CURVE", "How the fader cuts: SMOOTH, a mixing fade (-3 dB at the middle); CUT, a "
                                                       "scratch mixer's, fully open after 1% of its travel.");
    make (faderControls, ParamIDs::deckTransform, "TRANSFORM", "TRANSFORM's rate: held, a deck's key chops its side on and off this fast, "
                                                               "in time with the song (1/16: four times a beat).", true);
    make (faderControls, ParamIDs::deckHamster, "HAMSTER", "Turns the fader round, as many scratch DJs set theirs: DECK A's end "
                                                           "moves to the right.");
    // SCRATCH: MOD HAND and HAND.
    make (handControls, ParamIDs::deckModHand, "MOD HAND", "The mod wheel as a hand on HAND's record (it stays where you leave it, unlike "
                                                           "the pitch wheel's spring). On, the mod wheel is the deck's, not the synth's.");
    make (handControls, ParamIDs::deckHand, "HAND", "Which deck your hand is on: the one the pitch wheel (and MOD HAND) moves. (If that "
                                                    "deck's off and the other's on, the other.) Each turntable can be grabbed with the mouse.", true);

    // On the rack, in its pearl black (lettered in its ink whatever the window's finish).
    Finish::paintAs (Finish::Type::pearlBlack);
    for (auto& set : sets)
    {
        for (auto* list : { &set.deckControls, &set.cueControls, &set.scratchControls, &set.mixerControls, &set.killControls, &set.routingControls })
            for (auto& c : *list)
            {
                c->setRack (true);
                rack.addChildComponent (*c);
            }
        for (auto* c : { set.keysMenu.get(), set.scratchKeysMenu.get(), set.level.get() })
        {
            c->setRack (true);
            rack.addChildComponent (*c);
        }
    }
    for (auto* list : { &faderControls, &handControls })
        for (auto& c : *list)
        {
            c->setRack (true);
            rack.addChildComponent (*c);
        }
    Finish::paintAs (std::nullopt);

    // The rack's keys: DECK A and B (which deck it shows), ON (lit while the deck is on), LOAD, HOUSE.
    for (int d = 0; d < ParamIDs::numDecks; ++d)
    {
        auto& deckKey = deckKeys[(size_t) d];
        deckKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        deckKey.setComponentID (d == 0 ? "deckShowA" : "deckShowB");
        deckKey.setRadioGroupId (0x7d3c);
        deckKey.setClickingTogglesState (true);
        deckKey.setTooltip ("Shows DECK " + ParamIDs::deckNames[d] + " on the rack: its controls, its record on the screen; the keys under "
                            "the turntables play it. (Grabbing a turntable shows its deck too.)");
        deckKey.onClick = [this, d] { selectDeck (d); };
        rack.addAndMakeVisible (deckKey);

        auto& onKey = sets[(size_t) d].onKey;
        onKey.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        onKey.setComponentID (d == 0 ? "deckOn" : "deckBOn");
        onKey.setClickingTogglesState (true);
        onKey.setTooltip ("DECK " + ParamIDs::deckNames[d] + " on or off. On, its 12 keys (CUES > KEYS) play the deck, not the synth (and the "
                          "pitch wheel, while HAND is this deck); off, the synth has them all, as on PR7-K.");
        sets[(size_t) d].onAttachment = std::make_unique<juce::ButtonParameterAttachment> (*state.getParameter (ParamIDs::deckIds[d].on), onKey);
        rack.addChildComponent (onKey);
    }
    for (auto* key : { &loadKey, &houseKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
        rack.addAndMakeVisible (*key);
    }
    loadKey.setComponentID ("deckLoad");
    loadKey.setTooltip ("Puts a record on the deck the rack shows: a sample from a file (WAV, AIFF, FLAC or Ogg, up to 3 minutes). "
                        "The project and your presets remember it.");
    houseKey.setComponentID ("deckHouse");
    houseKey.setTooltip ("Puts THE HOUSE RECORD on the deck the rack shows, our own built-in scratch record: an \"ahh\", a horn stab, "
                         "a drum break and a 1 kHz tone, two seconds apart, at the cues' first places.");
    loadKey.onClick = [this]
    {
        const auto d = selected;
        chooser = std::make_unique<juce::FileChooser> ("Put a record on DECK " + ParamIDs::deckNames[d], juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this, d] (const juce::FileChooser& c)
                              {
                                  if (c.getResult().existsAsFile())
                                  {
                                      processor.loadDeckRecord (c.getResult(), d);
                                      processor.deckRecordChangedByUser();
                                  }
                                  timerCallback();
                              });
    };
    houseKey.onClick = [this] { processor.useHouseRecord (selected); processor.deckRecordChangedByUser(); timerCallback(); };
    rack.onPageChanged = [this] { showRackPage(); };
    // A technique clicked on the SCRATCH screen: its key pressed on the deck shown, as long as the mouse is down (as from MIDI).
    rack.onScratchPressed = [this] (int technique, bool down)
    {
        const auto first = processor.getDeckSettings (selected).firstScratchKey;
        if (first < 0 || processor.getParameters().getRawParameterValue (ParamIDs::deckIds[selected].on)->load() <= 0.5f)
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
        { deck::Key::cut, "The deck's side of THE CROSSFADER flipped while it's held: open to cut, or (cut) a tap that opens it. "
                          "With the hand: chirps, stabs." },
        { deck::Key::transform, "The deck's side of THE CROSSFADER chopped on and off in time with the song while it's held, at "
                                "TRANSFORM's rate (FADER page): a transformer scratch." } };
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
    rackButton.setTooltip ("Hides the rack (the turntables get the page's height) or shows it again.");
    rackButton.onClick = [this] { setRackShown (! rackShown); };
    addAndMakeVisible (rackButton);

    for (auto* label : { &speedLabel, &speedLabelB, &keysLabel })
    {
        label->setColour (juce::Label::textColourId, ParagonColours::textDim);
        addAndMakeVisible (*label);
    }
    for (auto* label : { &speedLabel, &speedLabelB })
    {
        label->setFont (juce::FontOptions (18.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, ParagonColours::accent);
    }
    keysLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    keysLabel.setMinimumHorizontalScale (0.7f);

    for (int d = 0; d < ParamIDs::numDecks; ++d)
    {
        getPlatter (d).onGrab = [this, d] { selectDeck (d); };
        addAndMakeVisible (getPlatter (d));
        addAndMakeVisible (getStrip (d));
    }
    addAndMakeVisible (crossfader);
    selectDeck (0);
    setRackShown (true);
    timerCallback();
    startTimerHz (20);
}

DeckPage::~DeckPage()
{
    for (int d = 0; d < ParamIDs::numDecks; ++d)
        processor.setDeckMouseHand (false, 0.0f, d);
}

void DeckPage::selectDeck (int d)
{
    // 0.9.0: the deck the rack shows and the keys play. Keys held on the other deck let go first (their note-offs).
    d = juce::jlimit (0, ParamIDs::numDecks - 1, d);
    if (d != selected)
        for (auto& k : keys)
            if (k.button->isDown())
                press (k.key, false);
    selected = d;
    deckKeys[(size_t) d].setToggleState (true, juce::dontSendNotification);
    for (int other = 0; other < ParamIDs::numDecks; ++other)
        getPlatter (other).setSelected (other == d);
    showRackPage();
    timerCallback();
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
    for (int d = 0; d < ParamIDs::numDecks; ++d)
    {
        auto& set = sets[(size_t) d];
        const auto mine = d == selected;
        for (auto& c : set.deckControls)
            c->setVisible (mine && page == DeckRack::Page::deck);
        for (auto& c : set.cueControls)
            c->setVisible (mine && page == DeckRack::Page::cues);
        set.keysMenu->setVisible (mine && page == DeckRack::Page::cues);
        for (auto& c : set.scratchControls)
            c->setVisible (mine && page == DeckRack::Page::scratch);
        set.scratchKeysMenu->setVisible (mine && page == DeckRack::Page::scratch);
        for (auto* list : { &set.mixerControls, &set.killControls })
            for (auto& c : *list)
                c->setVisible (mine && page == DeckRack::Page::mixer);
        for (auto& c : set.routingControls)
            c->setVisible (mine && page == DeckRack::Page::routing);
        set.level->setVisible (mine);
        set.onKey.setVisible (mine);
    }
    for (auto& c : faderControls)
        c->setVisible (page == DeckRack::Page::fader);
    for (auto& c : handControls)
        c->setVisible (page == DeckRack::Page::scratch);
}

void DeckPage::press (deck::Key key, bool down)
{
    const auto note = processor.getDeckSettings (selected).firstKey + (int) key;
    if (down)
        processor.getKeyboardState().noteOn (1, note, 1.0f);
    else
        processor.getKeyboardState().noteOff (1, note, 0.0f);
}

void DeckPage::timerCallback()
{
    const auto isOn = [this] (int d) { return processor.getParameters().getRawParameterValue (ParamIDs::deckIds[d].on)->load() > 0.5f; };
    const auto on = isOn (selected);
    for (auto& k : keys)
        k.button->setEnabled (on);   // off, the keys would play the synth

    // Under each turntable: how it's turning.
    for (int d = 0; d < ParamIDs::numDecks; ++d)
    {
        const auto speed = processor.getDeck (d).getSpeed();
        const auto text = ! isOn (d) ? juce::String ("DECK " + ParamIDs::deckNames[d] + " OFF")
                        : std::abs (speed) < 0.0005f ? juce::String ("STOPPED")
                        : juce::String (std::abs (speed) * 100.0f, 1) + "%" + (speed < 0.0f ? "  BACKWARDS" : "");
        (d == 0 ? speedLabel : speedLabelB).setText (text, juce::dontSendNotification);
    }

    // The rack's screen: the deck it shows, its record, how it's turning, where the needle is.
    const auto& deck = processor.getDeck (selected);
    const auto speed = deck.getSpeed();
    const auto settings = processor.getDeckSettings (selected);
    const auto record = processor.getDeckRecord (selected);
    const auto problem = processor.getDeckRecordProblem (selected);
    const auto hand = processor.handDeck() == selected;
    DeckRack::Screen screen;
    screen.caption = "DECK " + ParamIDs::deckNames[selected] + (processor.getDeckRecordFile (selected).isEmpty() ? "  -  THE RECORD: BUILT IN" : "  -  THE RECORD");
    screen.record = record != nullptr ? record->name : juce::String ("NO RECORD");
    screen.state = ! on ? juce::String ("OFF")
                 : deck.getScratch() >= 0 ? deck::scratchNames[deck.getScratch()]   // 0.5.0: the technique playing
                 : std::abs (speed) < 0.0005f ? juce::String ("STOPPED") : speed < 0.0f ? juce::String ("BACKWARDS") : juce::String ("PLAYING");
    screen.line1 = record != nullptr ? juce::String (deck.getPositionSeconds(), 1) + " / " + juce::String (record->seconds(), 1) + " S" : juce::String();
    screen.problem = problem.isNotEmpty();
    screen.line2 = screen.problem ? problem + ": THE HOUSE RECORD MEANWHILE"
                                  : "KEYS FROM " + noteName (settings.firstKey) + (hand ? "  -  PITCH WHEEL: THE HAND" : "  -  HAND: DECK " + ParamIDs::deckNames[1 - selected]);
    rack.setScreen (screen);
    rack.setOn (on);
    const auto peak = processor.takeDeckPeak (selected);
    for (int d = 0; d < ParamIDs::numDecks; ++d)
        if (d != selected)
            processor.takeDeckPeak (d);   // (emptied, so a deck shown later doesn't show an old peak)
    rack.setMeterDb (peak > 1.0e-5f ? juce::Decibels::gainToDecibels (peak) : -100.0f);
    rack.setFaderGain (deck.getFaderGain());
    rack.setFaderInfo ({ "DECK " + ParamIDs::deckNames[selected] + ":  CUT " + noteName (settings.firstKey + (int) deck::Key::cut)
                             + "    TRANSFORM " + noteName (settings.firstKey + (int) deck::Key::transform),
                         "CUT FLIPS ITS SIDE WHILE HELD; TRANSFORM CHOPS IT AT " + deck::transformNames[settings.transformRate],
                         settings.hamster ? juce::String ("HAMSTER: A AT THE RIGHT, B AT THE LEFT") : juce::String ("A AT THE FADER'S LEFT, B AT ITS RIGHT") });
    juce::StringArray keyMap;
    for (int k = 0; k < deck::numKeys; ++k)
        if (deck::keyNames[k].isNotEmpty())
            keyMap.add (noteName (settings.firstKey + k) + "  " + deck::keyNames[k]);
    rack.setKeyMap (keyMap);
    juce::StringArray scratches;
    for (int t = 0; t < deck::numScratches; ++t)
        scratches.add ((settings.firstScratchKey >= 0 ? noteName (settings.firstScratchKey + t) : juce::String ("--")) + "  " + deck::scratchNames[t]);
    rack.setScratchList (scratches, deck.getScratch());
    {
        // ROUTING's screen: whether the sends' effects are on (a send to one that's off does nothing), the bus.
        const auto& ids = ParamIDs::deckIds[selected];
        const auto value = [this] (const char* id) { return processor.getParameters().getRawParameterValue (id)->load(); };
        const auto bus = juce::roundToInt (value (ids.insert));
        rack.setRoutingInfo ({
            juce::String ("DELAY ") + (value (ParamIDs::delayOn) > 0.5f ? "ON" : "OFF: TURN IT ON (FX > SENDS)") + "   TO IT "
                + juce::String (juce::roundToInt (value (ids.sendDelay) * 100.0f)) + "%",
            juce::String ("REVERB ") + (value (ParamIDs::reverbOn) > 0.5f ? "ON" : "OFF: TURN IT ON (FX > SENDS)") + "   TO IT "
                + juce::String (juce::roundToInt (value (ids.sendReverb) * 100.0f)) + "%",
            bus > 0 ? "INSERT BUS " + ParamIDs::insertBusNames[bus] + ": ITS EFFECTS ON FX > INSERTS" : juce::String ("INSERT: NONE, STRAIGHT INTO THE MIX"),
            "THE SENDS TAKE THE DECK AFTER ITS FADER: CUT IT AND THE ECHOES RING ON",
            "BOTH DECKS' RECORDS ARE SAVED WITH YOUR PRESETS AND PROJECTS" });
    }

    juce::String legend ("DECK " + ParamIDs::deckNames[selected] + " KEYS:  ");
    for (const auto& k : keys)
        legend << noteName (settings.firstKey + (int) k.key) << " " << deck::keyNames[(int) k.key] << "   ";
    legend << "PITCH WHEEL: DECK " << ParamIDs::deckNames[processor.handDeck()];
    keysLabel.setText (legend, juce::dontSendNotification);
    if (! rackShown)
        repaint (0, 0, getWidth(), 44);   // the header names the records while the rack's hidden
}

std::vector<ParamControl*> DeckPage::getShownControls()
{
    std::vector<ParamControl*> result;
    for (auto& set : sets)
    {
        for (auto* list : { &set.deckControls, &set.cueControls, &set.scratchControls, &set.mixerControls, &set.killControls, &set.routingControls })
            for (auto& c : *list)
                result.push_back (c.get());
        result.push_back (set.keysMenu.get());
        result.push_back (set.scratchKeysMenu.get());
        result.push_back (set.level.get());
    }
    for (auto* list : { &faderControls, &handControls })
        for (auto& c : *list)
            result.push_back (c.get());
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
    g.drawText ("THE DECKS", juce::Rectangle<int> (24, 12, 120, 26), juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    juce::String names;
    for (int d = 0; d < ParamIDs::numDecks; ++d)
        if (const auto record = processor.getDeckRecord (d); record != nullptr)
            names << "DECK " << ParamIDs::deckNames[d] << ": " << record->name << "  (" << juce::String (record->seconds(), 1) << " s)     ";
    g.drawText (names, juce::Rectangle<int> (150, 12, 800, 26), juce::Justification::centredLeft, false);
}

void DeckPage::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    if (rackShown)
    {
        rack.setBounds (area.removeFromTop (DeckRack::preferredHeight));
        area.removeFromTop (4);
        // The rack's controls in the places it gives (both decks' in the same places: the one shown is visible).
        for (auto& set : sets)
        {
            for (auto* list : { &set.deckControls, &set.cueControls, &set.scratchControls, &set.routingControls })
                for (int i = 0; i < (int) list->size(); ++i)
                    (*list)[(size_t) i]->setBounds (rack.knobCell (i));
            // MIXER: CART, LOW, MID, HIGH, the KILLs stacked beside them, FILTER, RES, WEAR.
            for (int i = 0; i < (int) set.mixerControls.size(); ++i)
                set.mixerControls[(size_t) i]->setBounds (rack.knobCell (i < 4 ? i : i + 1));
            for (int band = 0; band < (int) set.killControls.size(); ++band)
                set.killControls[(size_t) band]->setBounds (rack.killCell (band));
            set.keysMenu->setBounds (rack.menuCell().reduced (6, 2));
            set.scratchKeysMenu->setBounds (rack.menuCell().reduced (6, 2));
            set.level->setBounds (rack.sideKnobCell());
            set.onKey.setBounds (rack.keyCell (0));
        }
        for (int i = 0; i < (int) faderControls.size(); ++i)
            faderControls[(size_t) i]->setBounds (rack.knobCell (i));
        // SCRATCH: each deck's REACH and RATE, then the mixer's MOD HAND and HAND.
        for (int i = 0; i < (int) handControls.size(); ++i)
            handControls[(size_t) i]->setBounds (rack.knobCell (2 + i));
        for (int d = 0; d < ParamIDs::numDecks; ++d)
            deckKeys[(size_t) d].setBounds (rack.deckKeyCell (d));
        loadKey.setBounds (rack.keyCell (1));
        houseKey.setBounds (rack.keyCell (2));
    }
    else
        area.removeFromTop (36);   // the header (painted)

    // 0.9.0: THE TURNTABLES, DECK A at the left and DECK B at the right as on a DJ's table, each at its picture's
    // shape, as big as the height allows (at most 30% of the width each: the mixer between them gets the rest; 32%
    // with the rack hidden: more and the keys between them wouldn't fit their names).
    const auto labelHeight = 28;
    const auto width = juce::jmin (area.getWidth() * (rackShown ? 30 : 32) / 100, juce::roundToInt ((float) (area.getHeight() - labelHeight) * DeckPlatter::pictureAspect));
    auto left = area.removeFromLeft (width);
    auto right = area.removeFromRight (width);
    speedLabel.setBounds (left.removeFromBottom (labelHeight));
    platter.setBounds (left);
    speedLabelB.setBounds (right.removeFromBottom (labelHeight));
    platterB.setBounds (right);
    area.reduce (14, 0);

    // Between them: HIDE / SHOW RACK, the records' strips, the shown deck's keys, the legend, the crossfader.
    auto top = area.removeFromTop (28);
    rackButton.setBounds ((rackShown ? top : juce::Rectangle<int> (getWidth() - 150, 12, 130, 28)).removeFromRight (130));
    area.removeFromTop (4);
    const auto keysHeight = 2 * 34 + 6;
    const auto faderHeight = juce::jmin (76, area.getHeight() / 6);
    const auto spare = area.getHeight() - (keysHeight + 8 + 20 + 8 + faderHeight + 3 * 8);
    const auto stripHeight = juce::jlimit (60, 130, spare / 2);
    strip.setBounds (area.removeFromTop (stripHeight));
    area.removeFromTop (8);
    stripB.setBounds (area.removeFromTop (stripHeight));
    area.removeFromTop (8);
    auto keyArea = area.removeFromTop (keysHeight);
    constexpr int perRow = 5;
    for (size_t k = 0; k < keys.size(); ++k)
    {
        auto row = k < perRow ? keyArea.withHeight (34) : keyArea.withTrimmedTop (40);
        const auto keyWidth = (row.getWidth() - (perRow - 1) * 5) / perRow;
        const auto column = (int) k % perRow;
        keys[k].button->setBounds (row.getX() + column * (keyWidth + 5), row.getY(), keyWidth, row.getHeight());
    }
    area.removeFromTop (8);
    keysLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (8);
    crossfader.setBounds (area.removeFromBottom (faderHeight));
}
