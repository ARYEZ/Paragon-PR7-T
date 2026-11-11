#include "DeckRack.h"

#include "ui/Finish.h"
#include "ui/LedDisplay.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/PedalBody.h"

namespace
{
    // As PR7-I's GuitarRack (PR7-G's rack units): the chassis, the room round it for its light, the screens' size.
    const PedalBody::Shape unitShape { 16.0f, 10.0f, 5.0f, 0.3f };
    constexpr float glowRoom = 7.0f;
    constexpr float screenSize = 0.45f;
    constexpr float sectionGap = 28.0f;    // between the sections, a printed red line in its middle
    constexpr float badgeWidth = 190.0f, displayWidth = 250.0f, sideWidth = 150.0f;
    constexpr float titleHeight = 24.0f;   // the page's name, over it
    constexpr float screenHeight = 118.0f;
    const auto orange = LedDisplay::ledOrange;

    juce::FontOptions font (float height, int style = juce::Font::bold) { return juce::FontOptions (height, style); }

    void stamped (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
                  juce::Justification justification = juce::Justification::centred)
    {
        g.setFont (font (height));
        ParagonLookAndFeel::drawStampedText (g, text, area, justification, Finish::ink());
    }

    // A red bracket printed over two keys with a word in its middle (as PR7-G's HOLD TO TUNER, PR7-I's KEY MAP).
    void bracket (juce::Graphics& g, juce::Rectangle<float> a, juce::Rectangle<float> b, const juce::String& word)
    {
        const auto y = a.getY() - 8.0f;
        g.setColour (Finish::printedRed());
        g.setFont (font (8.0f));
        const auto label = juce::Rectangle<float> (a.getCentreX(), y - 6.0f, b.getCentreX() - a.getCentreX(), 12.0f);
        g.drawText (word, label, juce::Justification::centred, false);
        const auto gap = juce::GlyphArrangement::getStringWidth (font (8.0f), word) * 0.5f + 6.0f;
        g.fillRect (juce::Rectangle<float> (a.getCentreX(), y, label.getWidth() * 0.5f - gap, 1.2f));
        g.fillRect (juce::Rectangle<float> (label.getCentreX() + gap, y, label.getWidth() * 0.5f - gap, 1.2f));
        g.fillRect (juce::Rectangle<float> (a.getCentreX(), y, 1.2f, 5.0f));
        g.fillRect (juce::Rectangle<float> (b.getCentreX() - 1.2f, y, 1.2f, 5.0f));
    }

    // A row of LED bars filled to `fraction` (the FADER screen's gain, across).
    void ledRow (juce::Graphics& g, juce::Rectangle<float> area, float fraction, int count)
    {
        const auto pitch = area.getWidth() / (float) count;
        const auto lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, fraction) * (float) count);
        for (int i = 0; i < count; ++i)
        {
            const auto bar = juce::Rectangle<float> (area.getX() + pitch * (float) i, area.getY(), pitch, area.getHeight()).reduced (pitch * 0.2f, 0.0f);
            if (i < lit)
            {
                g.setColour (orange.withAlpha (0.3f));
                g.fillRoundedRectangle (bar.expanded (2.0f), 2.0f);
                g.setColour (orange);
            }
            else
                g.setColour (orange.withAlpha (0.1f));
            g.fillRoundedRectangle (bar, 1.5f);
        }
    }
}

DeckRack::DeckRack()
{
    setOpaque (false);
    for (auto* key : { &deckKey, &faderKey, &cuesKey, &scratchKey, &mixerKey, &routingKey })
    {
        key->getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);   // a small key, its stripe lit on its page
        addAndMakeVisible (*key);
    }
    deckKey.setComponentID ("rackDeck");
    faderKey.setComponentID ("rackFader");
    cuesKey.setComponentID ("rackCues");
    scratchKey.setComponentID ("rackScratch");
    scratchKey.setTooltip ("SCRATCH: the scratch library (its keys, REACH, RATE, MOD HAND); click a technique to play it");
    scratchKey.onClick = [this] { setPage (Page::scratch); };
    mixerKey.setComponentID ("rackMixer");
    mixerKey.setTooltip ("MIXER: the cartridge, the mixer channel's EQ and KILLs, the FILTER, the record's WEAR");
    mixerKey.onClick = [this] { setPage (Page::mixer); };
    routingKey.setComponentID ("rackRouting");
    routingKey.setTooltip ("ROUTING: the deck into the FX: its sends to DELAY and REVERB, an insert bus");
    routingKey.onClick = [this] { setPage (Page::routing); };
    deckKey.setTooltip ("DECK: the turntable (PITCH, RPM, the motor's START and BRAKE, BACKSPIN, the wheel's REACH, the needle)");
    faderKey.setTooltip ("FADER: the crossfader (FADER, CURVE, TRANSFORM's rate, HAMSTER)");
    cuesKey.setTooltip ("CUES: where DROP 1-4 land, and which keys play the deck");
    deckKey.onClick = [this] { setPage (Page::deck); };
    faderKey.onClick = [this] { setPage (Page::fader); };
    cuesKey.onClick = [this] { setPage (Page::cues); };
    setPage (Page::deck);
}

void DeckRack::resized()
{
    satin = Finish::makeSatinGrain (juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), 1234);
    deckKey.setBounds (pageKeyCell (0));
    faderKey.setBounds (pageKeyCell (1));
    cuesKey.setBounds (pageKeyCell (2));
    scratchKey.setBounds (pageKeyCell (3));
    mixerKey.setBounds (pageKeyCell (4));
    routingKey.setBounds (pageKeyCell (5));
}

void DeckRack::setScreen (const Screen& s)
{
    if (s == screen)
        return;
    screen = s;
    repaint (screenArea());
}

void DeckRack::setOn (bool shouldBeOn)
{
    if (shouldBeOn == on)
        return;
    on = shouldBeOn;
    repaint();
}

void DeckRack::setMeterDb (float db)
{
    if (std::abs (db - meterDb) < 0.5f)
        return;
    meterDb = db;
    repaint (meterArea().expanded (4));
}

void DeckRack::setFaderGain (float gain)
{
    if (std::abs (gain - faderGain) < 0.02f)
        return;
    faderGain = gain;
    if (page == Page::fader)
        repaint (faderScreenArea());
}

void DeckRack::setFaderInfo (const juce::StringArray& lines)
{
    if (lines == faderInfo)
        return;
    faderInfo = lines;
    if (page == Page::fader)
        repaint (faderScreenArea());
}

void DeckRack::setKeyMap (const juce::StringArray& lines)
{
    if (lines == keyMap)
        return;
    keyMap = lines;
    if (page == Page::cues)
        repaint (keyMapArea());
}

void DeckRack::setPage (Page newPage)
{
    deckKey.setToggleState (newPage == Page::deck, juce::dontSendNotification);
    faderKey.setToggleState (newPage == Page::fader, juce::dontSendNotification);
    cuesKey.setToggleState (newPage == Page::cues, juce::dontSendNotification);
    scratchKey.setToggleState (newPage == Page::scratch, juce::dontSendNotification);
    mixerKey.setToggleState (newPage == Page::mixer, juce::dontSendNotification);
    routingKey.setToggleState (newPage == Page::routing, juce::dontSendNotification);
    if (newPage == page)
        return;
    page = newPage;
    repaint();
    if (onPageChanged != nullptr)
        onPageChanged();
}

//==============================================================================
juce::Rectangle<float> DeckRack::face() const
{
    return getLocalBounds().toFloat().reduced (glowRoom).withTrimmedBottom (unitShape.frontEdge).reduced (9.0f);
}

juce::Rectangle<float> DeckRack::content() const { return face().reduced (12.0f, 8.0f); }
juce::Rectangle<float> DeckRack::badgeColumn() const { return content().withWidth (badgeWidth); }
juce::Rectangle<float> DeckRack::displayColumn() const { return content().withTrimmedLeft (badgeWidth + sectionGap).withWidth (displayWidth); }
juce::Rectangle<float> DeckRack::sideColumn() const { return content().removeFromRight (sideWidth); }

juce::Rectangle<float> DeckRack::pageArea() const
{
    auto area = content();
    area.removeFromLeft (badgeWidth + sectionGap + displayWidth + sectionGap);
    area.removeFromRight (sideWidth + sectionGap);
    area.removeFromTop (titleHeight + 6.0f);
    return area;
}

juce::Rectangle<int> DeckRack::pageKeyCell (int index) const
{
    // 0.7.0: six pages, in two rows of three under the badge (PAGE printed over the top row), each key's name under it.
    const auto area = badgeColumn().removeFromBottom (102.0f);
    const auto width = area.getWidth() / 3.0f;
    const auto top = area.getY() + 12.0f + 46.0f * (float) (index / 3);
    return juce::Rectangle<float> (area.getX() + width * (float) (index % 3), top, width, 26.0f).withSizeKeepingCentre (44.0f, 26.0f).toNearestInt();
}

juce::Rectangle<int> DeckRack::screenArea() const { return displayColumn().withHeight (screenHeight).toNearestInt(); }

juce::Rectangle<int> DeckRack::keyCell (int index) const
{
    auto area = displayColumn().withTrimmedTop (screenHeight + 14.0f + 14.0f);   // RECORD printed over the last two
    const auto width = area.getWidth() / 3.0f;
    return area.withX (area.getX() + width * (float) index).withWidth (width).withTrimmedBottom (12.0f)
               .withSizeKeepingCentre (54.0f, 28.0f).toNearestInt();
}

juce::Rectangle<int> DeckRack::killCell (int band) const
{
    const auto cell = knobCell (4);
    const auto height = cell.getHeight() / 3;
    return cell.withY (cell.getY() + height * band).withHeight (height);
}

juce::Rectangle<int> DeckRack::knobCell (int index) const
{
    const auto area = pageArea();
    const auto width = area.getWidth() / (float) numKnobs;
    const auto height = juce::jmin (area.getHeight(), 136.0f);
    return juce::Rectangle<float> (area.getX() + width * (float) index, area.getCentreY() - height * 0.5f, width, height)
        .reduced (4.0f, 0.0f).toNearestInt();
}

// FADER and CUES: four knobs on the left, then what's beside them.
juce::Rectangle<int> DeckRack::faderScreenArea() const
{
    auto area = pageArea().withTrimmedLeft (pageArea().getWidth() * 0.5f + 12.0f);
    return area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 140.0f)).toNearestInt();
}

juce::Rectangle<int> DeckRack::menuCell() const
{
    const auto area = pageArea().withTrimmedLeft (pageArea().getWidth() * 0.5f + 12.0f);
    return juce::Rectangle<float> (area.getX(), area.getY() + 16.0f, 120.0f, 30.0f).toNearestInt();
}

juce::Rectangle<int> DeckRack::keyMapArea() const
{
    const auto area = pageArea().withTrimmedLeft (pageArea().getWidth() * 0.5f + 12.0f);
    return area.withTrimmedTop (60.0f).toNearestInt();
}

juce::Rectangle<int> DeckRack::meterArea() const
{
    return sideColumn().withTrimmedTop (titleHeight + 6.0f).withWidth (50.0f).toNearestInt();
}

juce::Rectangle<int> DeckRack::sideKnobCell() const
{
    const auto area = sideColumn().withTrimmedTop (titleHeight + 6.0f).withTrimmedLeft (50.0f + 8.0f);
    return area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 136.0f)).reduced (2.0f, 0.0f).toNearestInt();
}

//==============================================================================
void DeckRack::paint (juce::Graphics& g)
{
    // The unit, and everything on it (its knobs and keys too, till paintOverChildren), in pearl black.
    Finish::paintAs (Finish::Type::pearlBlack);

    const auto chassis = getLocalBounds().toFloat().reduced (glowRoom);
    PedalBody::paintUnit (g, chassis, face(), [this] (juce::Graphics& gg) { gg.drawImage (satin, getLocalBounds().toFloat()); }, unitShape);

    // The badge, as on PR7-G's and PR7-I's units: PARAGON over the PR7-T mark, what it is printed in red.
    PedalBody::paintBadge (g, badgeColumn().withHeight (88.0f).translated (2.0f, 2.0f), "TURNTABLE INSTRUMENT");
    {
        const char* names[] = { "DECK", "FADER", "CUES", "SCRATCH", "MIXER", "ROUTING" };
        bracket (g, pageKeyCell (0).toFloat(), pageKeyCell (2).toFloat(), "PAGE");
        for (int k = 0; k < 6; ++k)
        {
            const auto key = pageKeyCell (k).toFloat();
            stamped (g, names[k], key.translated (0.0f, key.getHeight() + 2.0f).withHeight (12.0f).expanded (12.0f, 0.0f), 7.0f);
        }
    }

    // The printed lines between the sections.
    const auto printedLine = [&] (float x)
    {
        g.setColour (Finish::printedRed().withAlpha (0.7f));
        g.fillRect (juce::Rectangle<float> (x - 0.75f, face().getY() + 14.0f, 1.5f, face().getHeight() - 28.0f));
    };
    printedLine (displayColumn().getX() - sectionGap * 0.5f);
    printedLine (displayColumn().getRight() + sectionGap * 0.5f);
    printedLine (sideColumn().getX() - sectionGap * 0.5f);

    paintScreen (g);

    // The keys' names stamped under them; RECORD printed over the last two.
    for (int i = 0; i < 3; ++i)
    {
        const auto key = keyCell (i).toFloat();
        stamped (g, i == 0 ? "ON" : i == 1 ? "LOAD" : "HOUSE", key.translated (0.0f, key.getHeight() + 2.0f).withHeight (12.0f).expanded (20.0f, 0.0f), 8.0f);
    }
    bracket (g, keyCell (1).toFloat(), keyCell (2).toFloat(), "RECORD");

    // The page's name, over it.
    auto title = pageArea().withY (content().getY()).withHeight (titleHeight);
    g.setFont (font (15.0f, juce::Font::bold | juce::Font::italic));
    ParagonLookAndFeel::drawStampedText (g, page == Page::deck ? "DECK" : page == Page::fader ? "FADER" : page == Page::cues ? "CUES"
                                          : page == Page::scratch ? "SCRATCH" : page == Page::mixer ? "MIXER" : "ROUTING",
                                         title.removeFromLeft (page == Page::scratch || page == Page::routing ? 110.0f : 90.0f), juce::Justification::centredLeft, Finish::ink());
    g.setColour (Finish::printedRed());
    g.setFont (font (8.0f));
    g.drawText (page == Page::deck ? "THE TURNTABLE: ITS SPEED, ITS MOTOR, THE HAND, THE NEEDLE"
              : page == Page::fader ? "THE CROSSFADER: HOW IT CUTS, TRANSFORM'S RATE, HAMSTER"
              : page == Page::cues ? "WHERE THE DROPS LAND, AND THE DECK'S KEYS"
              : page == Page::scratch ? "THE SCRATCH LIBRARY: A TECHNIQUE A KEY, IN TIME WITH THE SONG"
              : page == Page::mixer ? "THE CARTRIDGE, THE MIXER CHANNEL'S EQ AND KILLS, ITS FILTER, THE RECORD'S WEAR"
                                    : "THE DECK INTO THE FX: ITS SENDS TO DELAY AND REVERB, AN INSERT BUS",
                title, juce::Justification::centredLeft, false);

    if (page == Page::fader)
        paintFaderScreen (g);
    if (page == Page::routing)
        paintRoutingScreen (g);
    if (page == Page::cues || page == Page::scratch)
    {
        const auto cell = menuCell().toFloat();
        stamped (g, page == Page::cues ? "KEYS" : "SCRATCH KEYS", cell.withHeight (12.0f).translated (0.0f, -15.0f).withWidth (160.0f), 8.0f, juce::Justification::centredLeft);
        LedDisplay::paintScreen (g, cell, screenSize);
        if (page == Page::cues)
            paintKeyMap (g);
        else
            paintScratchList (g);
    }

    // DECK OUT: its name, the status bar (off while the deck is), the meter screen.
    auto sideTitle = sideColumn().withHeight (titleHeight);
    ParagonLookAndFeel::drawNeonBar (g, sideTitle.removeFromRight (30.0f).withSizeKeepingCentre (26.0f, 6.0f), on);
    g.setFont (font (15.0f, juce::Font::bold | juce::Font::italic));
    ParagonLookAndFeel::drawStampedText (g, "DECK OUT", sideTitle, juce::Justification::centredLeft, Finish::ink());
    paintMeter (g);
}

void DeckRack::paintScreen (juce::Graphics& g)
{
    const auto glass = LedDisplay::paintScreen (g, screenArea().toFloat(), screenSize);
    auto text = glass.reduced (12.0f, 8.0f);
    g.setColour (orange.withAlpha (0.6f));
    g.setFont (font (8.5f));
    g.drawText (screen.caption, text.removeFromTop (11.0f), juce::Justification::centredLeft, false);
    text.removeFromTop (3.0f);
    g.setColour (orange);
    g.setFont (font (21.0f));
    g.drawFittedText (screen.record, text.removeFromTop (26.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    text.removeFromTop (3.0f);
    g.setColour (orange.withAlpha (0.25f));
    g.fillRect (text.removeFromTop (1.0f));
    text.removeFromTop (5.0f);
    g.setColour (orange);
    g.setFont (font (13.0f));
    auto stateRow = text.removeFromTop (17.0f);
    g.drawText (screen.line1, stateRow, juce::Justification::centredRight, false);
    g.drawText (screen.state, stateRow, juce::Justification::centredLeft, false);
    g.setColour (screen.problem ? orange : orange.withAlpha (0.65f));
    g.setFont (font (9.5f));
    g.drawFittedText (screen.line2, text.removeFromTop (15.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::paintFaderScreen (juce::Graphics& g)
{
    const auto glass = LedDisplay::paintScreen (g, faderScreenArea().toFloat(), screenSize);
    auto text = glass.reduced (12.0f, 9.0f);
    g.setColour (orange.withAlpha (0.6f));
    g.setFont (font (8.5f));
    auto captionRow = text.removeFromTop (11.0f);
    g.drawText ("THE CROSSFADER", captionRow, juce::Justification::centredLeft, false);
    g.drawText (faderGain <= 0.001f ? "CUT" : faderGain >= 0.999f ? "OPEN" : juce::String (juce::roundToInt (faderGain * 100.0f)) + "%",
                captionRow, juce::Justification::centredRight, false);
    text.removeFromTop (6.0f);
    ledRow (g, text.removeFromTop (18.0f), faderGain, 24);
    text.removeFromTop (8.0f);
    for (int i = 0; i < faderInfo.size(); ++i)
    {
        g.setColour (i == 0 ? orange : orange.withAlpha (0.65f));
        g.setFont (font (i == 0 ? 11.0f : 9.5f));
        g.drawFittedText (faderInfo[i], text.removeFromTop (i == 0 ? 16.0f : 14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    }
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::paintKeyMap (juce::Graphics& g)
{
    const auto glass = LedDisplay::paintScreen (g, keyMapArea().toFloat(), screenSize);
    auto text = glass.reduced (10.0f, 7.0f);
    g.setColour (orange.withAlpha (0.6f));
    g.setFont (font (8.0f));
    g.drawText ("THE DECK'S KEYS", text.removeFromTop (10.0f), juce::Justification::centredLeft, false);
    text.removeFromTop (3.0f);
    const auto rows = (keyMap.size() + 1) / 2;
    const auto rowHeight = rows > 0 ? juce::jmin (12.0f, text.getHeight() / (float) rows) : 12.0f;
    const auto columnWidth = text.getWidth() / 2.0f;
    g.setColour (orange);
    g.setFont (font (juce::jmin (9.5f, rowHeight - 1.5f)));
    for (int i = 0; i < keyMap.size(); ++i)
        g.drawText (keyMap[i], juce::Rectangle<float> (text.getX() + columnWidth * (float) (i / rows), text.getY() + rowHeight * (float) (i % rows),
                                                       columnWidth, rowHeight), juce::Justification::centredLeft, false);
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::setRoutingInfo (const juce::StringArray& lines)
{
    if (lines == routingInfo)
        return;
    routingInfo = lines;
    if (page == Page::routing)
        repaint (faderScreenArea());
}

void DeckRack::paintRoutingScreen (juce::Graphics& g)
{
    const auto glass = LedDisplay::paintScreen (g, faderScreenArea().toFloat(), screenSize);
    auto text = glass.reduced (12.0f, 9.0f);
    g.setColour (orange.withAlpha (0.6f));
    g.setFont (font (8.5f));
    g.drawText ("WHERE THE DECK GOES", text.removeFromTop (11.0f), juce::Justification::centredLeft, false);
    text.removeFromTop (6.0f);
    for (int i = 0; i < routingInfo.size(); ++i)
    {
        g.setColour (i < 3 ? orange : orange.withAlpha (0.65f));
        g.setFont (font (i < 3 ? 11.0f : 9.5f));
        g.drawFittedText (routingInfo[i], text.removeFromTop (i < 3 ? 18.0f : 15.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::setScratchList (const juce::StringArray& lines, int playing)
{
    if (lines == scratchList && playing == scratchPlaying)
        return;
    scratchList = lines;
    scratchPlaying = playing;
    if (page == Page::scratch)
        repaint (keyMapArea());
}

// The techniques in two columns of six on their screen; each line a place to click.
juce::Rectangle<float> DeckRack::scratchRow (int index) const
{
    auto text = keyMapArea().toFloat().reduced (10.0f, 7.0f).withTrimmedTop (13.0f);
    const auto rows = 6;
    const auto rowHeight = text.getHeight() / (float) rows;
    const auto columnWidth = text.getWidth() / 2.0f;
    return { text.getX() + columnWidth * (float) (index / rows), text.getY() + rowHeight * (float) (index % rows), columnWidth, rowHeight };
}

int DeckRack::scratchAt (juce::Point<int> point) const
{
    if (page != Page::scratch)
        return -1;
    for (int i = 0; i < scratchList.size(); ++i)
        if (scratchRow (i).contains (point.toFloat()))
            return i;
    return -1;
}

void DeckRack::paintScratchList (juce::Graphics& g)
{
    const auto glass = LedDisplay::paintScreen (g, keyMapArea().toFloat(), screenSize);
    g.setColour (orange.withAlpha (0.6f));
    g.setFont (font (8.0f));
    g.drawText ("THE TECHNIQUES: CLICK ONE TO PLAY IT", glass.reduced (10.0f, 7.0f).withHeight (10.0f), juce::Justification::centredLeft, false);
    for (int i = 0; i < scratchList.size(); ++i)
    {
        const auto row = scratchRow (i);
        if (i == scratchPlaying)
        {
            g.setColour (orange.withAlpha (0.28f));
            g.fillRoundedRectangle (row.reduced (0.0f, 1.0f).withTrimmedRight (6.0f), 2.0f);
        }
        g.setColour (i == scratchPlaying ? juce::Colour (0xffffd0a0) : orange);
        g.setFont (font (juce::jmin (9.5f, row.getHeight() - 2.0f)));
        g.drawText (scratchList[i], row.withTrimmedLeft (3.0f), juce::Justification::centredLeft, false);
    }
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::mouseDown (const juce::MouseEvent& e)
{
    scratchClicked = scratchAt (e.getPosition());
    if (scratchClicked >= 0 && onScratchPressed != nullptr)
        onScratchPressed (scratchClicked, true);
}

void DeckRack::mouseUp (const juce::MouseEvent&)
{
    if (scratchClicked >= 0 && onScratchPressed != nullptr)
        onScratchPressed (scratchClicked, false);
    scratchClicked = -1;
}

void DeckRack::paintMeter (juce::Graphics& g)
{
    // A ladder of LED bars, -60 to +6 dB, the top two (over 0 dB) paler (PR7-I's).
    const auto glass = LedDisplay::paintScreen (g, meterArea().toFloat(), screenSize);
    auto bars = glass.reduced (9.0f, 10.0f);
    constexpr int count = 20;
    const auto pitch = bars.getHeight() / (float) count;
    const auto lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, (meterDb + 60.0f) / 66.0f) * (float) count);
    for (int i = 0; i < count; ++i)
    {
        const auto bar = juce::Rectangle<float> (bars.getX(), bars.getBottom() - pitch * (float) (i + 1), bars.getWidth(), pitch).reduced (0.0f, pitch * 0.22f);
        const auto colour = i >= count - 2 ? juce::Colour (0xffffb070) : orange;
        if (i < lit)
        {
            g.setColour (colour.withAlpha (0.3f));
            g.fillRoundedRectangle (bar.expanded (2.0f), 2.0f);
            g.setColour (colour);
        }
        else
            g.setColour (colour.withAlpha (0.1f));
        g.fillRoundedRectangle (bar, 1.5f);
    }
    LedDisplay::paintGlassOverlay (g, glass, screenSize);
}

void DeckRack::paintOverChildren (juce::Graphics& g)
{
    // CUES: the glass over the KEYS menu's screen (its text sits behind it, as on any of the unit's screens).
    if (page == Page::cues || page == Page::scratch)
        LedDisplay::paintGlassOverlay (g, menuCell().toFloat().reduced (3.0f), screenSize);
    Finish::paintAs (std::nullopt);
}
