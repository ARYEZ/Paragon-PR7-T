#pragma once

#include <optional>

#include <juce_gui_basics/juce_gui_basics.h>

// The unit's finish (0.10.0, as PR7-G 0.60.0 and PR7-B): OBSIDIAN (a pearl
// black unit, the default) or AURORA (pearl white). On both the keys and knobs
// are black, in neon rings (pink to red to orange): the small keys' surrounds,
// the knobs' rings, the big keys' bezels and the wheels' ribs. Only the unit
// itself changes (its chassis, the lettering printed on it); the screens, the
// neon and the dark panels round it stay as they are. (0.9.1 and before also
// had Pearl White, with chrome rings: retired.)
//
// It's a preference of the person, not part of a project or a rig: kept in a
// small settings file in the user's application data folder, so every
// PR7-T opened afterwards, in any project, uses the same finish. Every open
// window repaints when it changes.
namespace Finish
{
    // Saved by number: only ever add to the end. retiredPearlWhite (0) is only
    // ever read from an old settings file, and opens as Obsidian; obsidian (1)
    // was Pearl Black, aurora (2) Pearl White + Neon.
    enum class Type { retiredPearlWhite, obsidian, aurora };

    Type get();
    // PR7-T 0.4.0, from PR7-I: while a component paints (its children too), draw as this finish; nullopt to stop.
    // THE DECK's rack is always Obsidian (pearl black), whatever the finish, as PR7-I's rack is on its dark page.
    void paintAs (std::optional<Type>);
    // `remember` saves it to the settings file (off for tests and snapshots).
    void set (Type, bool remember = true);
    inline bool isBlack() { return get() == Type::obsidian; }            // the chassis colour

    // Told whenever the finish changes, to repaint.
    juce::ChangeBroadcaster& changes();

    // 0.49.0: whether the SYNTH page shows the rack (the OSCILLATORS and FILTER panels) or the front panel,
    // remembered in the same settings file, for every PR7-T window.
    bool rackShown();
    void setRackShown (bool);

    // Colours that follow the finish.
    juce::Colour ink();          // lettering printed on the unit: near black on white, pearl white on black
    juce::Colour inkDim();
    juce::Colour stampEdge();    // the fine edge under stamped lettering: a light catch on white, a shadow on black
    juce::Colour printedRed();   // the red lettering and lines (a touch brighter on black, so it reads as clearly)

    // The satin grain laid over pearl surfaces: a fine speckle with no
    // direction to it (so no brushed lines) and the odd bright mica fleck.
    // On black the flecks show more, as they do in a real black pearl paint.
    juce::Image makeSatinGrain (int width, int height, int seed);

    // The neon of the rings round the keys and knobs and of the wheels' ribs:
    // hot pink at the left through red to orange at the right (the reference swatch's
    // colours, 25% darker), across `left` to `right`. Lighting is drawn over it.
    juce::ColourGradient neonGradient (float left, float right);

    // AURORA's pearl is darkened by this much (the owner, on PR7-B: full white was
    // "too bright, almost disorienting"): every pixel of the pearl surfaces, their
    // sheen and highlights included, is drawn at 90% of its old value, so pure
    // white becomes 230/255. The lettering, the neon, the keys and the screens on
    // it keep their own colours. (PR7-B tried 25%, which looked grey; the owner
    // chose 10% from mock-ups of 10, 15 and 25%.)
    inline constexpr float auroraDimming = 0.10f;

    // Darkens a pearl surface just drawn, inside `area`, by auroraDimming on
    // Aurora (black laid over it: each colour times 1 - auroraDimming); nothing on Obsidian.
    void dimPearl (juce::Graphics&, const juce::Path& area);
    // A colour of Aurora's pearl as drawn (times 1 - auroraDimming); unchanged on Obsidian.
    juce::Colour pearl (juce::Colour);

    // Picks the colour for the current finish.
    inline juce::Colour pick (juce::uint32 onWhite, juce::uint32 onBlack) { return juce::Colour (isBlack() ? onBlack : onWhite); }
}
