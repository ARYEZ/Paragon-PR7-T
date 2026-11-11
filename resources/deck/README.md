# THE TURNTABLE's pictures (PR7-T 0.3.0)

Our own direct-drive DJ turntable, modelled and rendered in the PR7 family's "Tron" look, the way PR7-D's kit, PR7-I's
guitar and PR7-O's strings were (their materials, red grid floor, lights and fog glow come from PR7-I's
`resources/guitar/guitar3d.py`): a gloss black plinth with a glowing edge, a black chrome platter with a neon rim and
strobe dots, black vinyl with a glowing label ring and "PR7-T" in the plugin's own lettering, a tonearm with a neon
strip, the pitch fader, START / STOP and 33 / 45, on a gloss floor with a red grid. The neon runs across the deck pink
(left) to red to orange (right), as PR7-D's kit does. No maker's design, name or logo: our own layout.

Built into the plugin as `DeckImages::` binary data (see `CMakeLists.txt`) and drawn by `source/ui/DeckPage.cpp`
(`DeckPlatter`).

| File | What |
|---|---|
| `deck.jpg` | The deck from straight above, 1400 x 1080 (the plugin's 2x), the arm left out |
| `deck-lit.jpg` | The same with its neon about 2.6 times as bright: the page fades it in as the record turns |
| `platter.png` | The platter and record alone, cut to what's in them, with alpha: the page turns it round its centre |
| `arm.png` | The tonearm alone, cut likewise: the page swings it round its pivot so the needle follows the record |
| `deck.json` | Where the platter's centre, the record's radius, the arm's pivot and length, the stylus and the grooves are (picture px), and where each layer was cut from |

The camera looks straight down (orthographic, no tilt), so the turning layers line up exactly with the deck under
them however far they turn.

## Making them again

Needs Blender 4.0 (`apt-get install -y blender`) and Python 3 with numpy and Pillow. From this folder:

```bash
mkdir -p raw
for pass in base lit platter arm; do blender -b -P turntable3d.py -- $PWD/raw/$pass.png $pass 64 1400 1080 $PWD/raw/geometry.json; done
python3 process_deck.py raw .
```

- `turntable3d.py`: the scene. Arguments: output, pass (`base`: the deck; `lit`: its neon brighter; `platter`, `arm`:
  that layer alone on a transparent background), samples, width, height, and where to write the geometry.
  `DECK_LIGHT` sets the lights' brightness (0.2: dark, as PR7-I's guitar; the neon is emission and doesn't change).
- `process_deck.py`: every hue into the family's range (330-30 degrees, PR7-I's `family_hues`: Filmic burns the
  brightest orange towards yellow and the glow turns pink towards purple), the layers cut to their contents, the
  geometry written with where each was cut from.

Each pass takes about two minutes on 4 CPU cores at 64 samples.
