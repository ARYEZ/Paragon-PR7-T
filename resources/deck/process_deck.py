# Turns THE DECK's renders (turntable3d.py) into the page's pictures (PR7-T 0.3.0), as PR7-I's process_guitar.py does
# its guitar: every hue pulled into the family's range (pink through red to orange: the render's tone mapping burns the
# brightest orange toward yellow, the glow turns pink toward purple).
#   deck.jpg, deck-lit.jpg   the deck (base pass) and the same with its neon brighter (lit pass)
#   platter.png, arm.png     the turning layers, cut to what's in them, with their alpha
#   deck.json                the geometry (turntable3d.py's), and where each layer was cut from
# python3 process_deck.py <renders folder> <out folder>
#   (the renders folder holds base.png, lit.png, platter.png, arm.png and geometry.json)
import sys, os, json
import numpy as np
from PIL import Image

SRC, OUT = sys.argv[1], sys.argv[2]
LOW, HIGH = 330.0, 30.0     # the hues kept (degrees, through 0), inside the family's 318-42: JPEG drifts a few degrees

def family_hues(rgb):
    # PR7-I's: any pixel whose hue is outside LOW..HIGH (through red) is turned to the nearer end, keeping its
    # saturation and brightness: a yellow glint becomes orange, a purple haze pink.
    f = rgb.astype(np.float64) / 255.0
    mx, mn = f.max(axis=2), f.min(axis=2)
    c = mx - mn
    r, g, b = f[..., 0], f[..., 1], f[..., 2]
    h = np.zeros_like(mx)
    nz = c > 1e-9
    rm = nz & (mx == r)
    gm = nz & (mx == g) & ~rm
    bm = nz & ~rm & ~gm
    h[rm] = ((g[rm] - b[rm]) / c[rm]) % 6
    h[gm] = (b[gm] - r[gm]) / c[gm] + 2
    h[bm] = (r[bm] - g[bm]) / c[bm] + 4
    h *= 60.0
    out = (h > HIGH) & (h < LOW) & nz
    h2 = np.where(h <= 180.0, HIGH, LOW)
    hh = np.where(out, h2, h) / 60.0
    x = c * (1 - np.abs(hh % 2 - 1))
    zeros = np.zeros_like(c)
    sector = np.floor(hh).astype(int) % 6
    rr = np.select([sector == k for k in range(6)], [c, x, zeros, zeros, x, c])
    gg = np.select([sector == k for k in range(6)], [x, c, c, x, zeros, zeros])
    bb = np.select([sector == k for k in range(6)], [zeros, zeros, x, c, c, x])
    rebuilt = np.stack([rr + mn, gg + mn, bb + mn], axis=2)
    result = np.where(out[..., None], rebuilt, f)
    return np.clip(np.round(result * 255.0), 0, 255).astype(np.uint8), int(out.sum())

os.makedirs(OUT, exist_ok=True)
geometry = json.load(open(os.path.join(SRC, "geometry.json")))

for name, out in (("base", "deck.jpg"), ("lit", "deck-lit.jpg")):
    picture, moved = family_hues(np.array(Image.open(os.path.join(SRC, name + ".png")).convert("RGB")))
    print("%s: %d pixels turned into the family's hues" % (name, moved))
    Image.fromarray(picture).save(os.path.join(OUT, out), quality=88, subsampling=0, optimize=True)

geometry["layers"] = {}
for name in ("platter", "arm"):
    image = np.array(Image.open(os.path.join(SRC, name + ".png")).convert("RGBA"))
    rgb, moved = family_hues(image[..., :3])
    alpha = image[..., 3]
    ys, xs = np.nonzero(alpha > 0)
    x0, y0, x1, y1 = int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1
    cut = np.dstack([rgb, alpha])[y0:y1, x0:x1]
    Image.fromarray(cut, "RGBA").save(os.path.join(OUT, name + ".png"), optimize=True)
    geometry["layers"][name] = [x0, y0, x1 - x0, y1 - y0]
    print("%s: %d pixels turned, cut to %d x %d at %d, %d" % (name, moved, x1 - x0, y1 - y0, x0, y0))

with open(os.path.join(OUT, "deck.json"), "w") as f:
    json.dump(geometry, f, indent=1)
