# THE DECK's 3D turntable (PR7-T 0.3.0): our own direct-drive DJ turntable, modelled and rendered in the PR7 family's
# "Tron" look, as PR7-D's kit, PR7-I's guitar and PR7-O's strings were (their materials, floor, lights and glow, from
# PR7-I's guitar3d.py): a gloss black plinth with a glowing edge, a black chrome platter with a neon rim and strobe
# dots, black vinyl with a glowing label ring, a tonearm with a neon strip, the pitch fader, START / STOP and 33 / 45,
# on a gloss floor with a red grid. The neon's colour runs across the deck by world X: pink at the left, red in the
# middle, orange at the right (PR7-D's kit runs the same way). No maker's design, name or logo: our own layout.
#
# Seen from straight above (an orthographic camera, no tilt), so the plugin can turn the platter and swing the arm as
# flat pictures that line up exactly with the deck under them. Passes:
#   base      the whole deck: plinth, platter, record and arm's base on the floor (the arm itself left out)
#   lit       the same with the neon brighter (the page fades it in as the record turns)
#   platter   the platter and record alone on a transparent background (the page turns it)
#   arm       the tonearm alone on a transparent background (the page swings it about its pivot)
# blender -b -P turntable3d.py -- <out.png> <pass> [samples] [width] [height] [geometry.json]
import bpy, math, sys, json, os
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
OUT = argv[0] if argv else "/tmp/deck.png"
PASS = argv[1] if len(argv) > 1 else "base"
SAMPLES = int(argv[2]) if len(argv) > 2 else 32
WIDTH = int(argv[3]) if len(argv) > 3 else 1400
HEIGHT = int(argv[4]) if len(argv) > 4 else 1080
GEOMETRY = argv[5] if len(argv) > 5 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

# ================================================================ materials (PR7-I's guitar3d.py)
def srgb(h):
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c) + (1.0,)

def setin(node, names, value):
    for n in names:
        if n in node.inputs:
            node.inputs[n].default_value = value
            return

def material(name, colour, metallic=0.0, rough=0.4, coat=0.0, spec=0.5, emission=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = colour
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    setin(b, ("Coat Weight", "Clearcoat"), coat)
    setin(b, ("Coat Roughness",), 0.09)
    setin(b, ("Specular IOR Level", "Specular"), spec)
    if emission is not None:
        setin(b, ("Emission Color", "Emission"), emission)
        setin(b, ("Emission Strength",), strength)
    return m

def emission(name, colour, strength):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = colour
    em.inputs["Strength"].default_value = strength
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    return m

LIT = 2.6 if PASS == "lit" else 1.0   # the lit pass: the neon about 2.6 times as bright

def neon(name, strength):
    # Pink at the deck's left, red in the middle, orange at its right, by each point's world X.
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Strength"].default_value = strength * LIT
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    rng = nt.nodes.new("ShaderNodeMapRange")
    rng.inputs["From Min"].default_value = -0.23
    rng.inputs["From Max"].default_value = 0.23
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    e = ramp.color_ramp.elements
    e[0].position, e[0].color = 0.0, srgb("ff2d6e")
    e[1].position, e[1].color = 1.0, srgb("ff6a24")
    mid = e.new(0.5)
    mid.color = srgb("ff2a2e")
    nt.links.new(geo.outputs["Position"], sep.inputs["Vector"])
    nt.links.new(sep.outputs["X"], rng.inputs["Value"])
    nt.links.new(rng.outputs["Result"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    return m

def grid_floor():
    # Gloss black with a faint red grid (a line every 20 mm), fading out from the deck: PR7-I's floor.
    m = material("grid floor", srgb("030203"), rough=0.25, spec=0.5)
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    coords = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    nt.links.new(coords.outputs["Object"], sep.inputs["Vector"])
    def line(axis):
        a = nt.nodes.new("ShaderNodeMath"); a.operation = "MULTIPLY"; a.inputs[1].default_value = 50.0
        f = nt.nodes.new("ShaderNodeMath"); f.operation = "FRACT"
        c = nt.nodes.new("ShaderNodeMath"); c.operation = "SUBTRACT"; c.inputs[1].default_value = 0.5
        ab = nt.nodes.new("ShaderNodeMath"); ab.operation = "ABSOLUTE"
        g = nt.nodes.new("ShaderNodeMath"); g.operation = "GREATER_THAN"; g.inputs[1].default_value = 0.47
        nt.links.new(sep.outputs[axis], a.inputs[0]); nt.links.new(a.outputs[0], f.inputs[0])
        nt.links.new(f.outputs[0], c.inputs[0]); nt.links.new(c.outputs[0], ab.inputs[0]); nt.links.new(ab.outputs[0], g.inputs[0])
        return g
    mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MAXIMUM"
    nt.links.new(line("X").outputs[0], mx.inputs[0]); nt.links.new(line("Y").outputs[0], mx.inputs[1])
    dist = nt.nodes.new("ShaderNodeVectorMath"); dist.operation = "LENGTH"
    nt.links.new(coords.outputs["Object"], dist.inputs[0])
    fade = nt.nodes.new("ShaderNodeMapRange")
    fade.inputs["From Min"].default_value = 0.2; fade.inputs["From Max"].default_value = 0.45
    fade.inputs["To Min"].default_value = 0.18; fade.inputs["To Max"].default_value = 0.03
    nt.links.new(dist.outputs["Value"], fade.inputs["Value"])
    k = nt.nodes.new("ShaderNodeMath"); k.operation = "MULTIPLY"
    nt.links.new(mx.outputs[0], k.inputs[0]); nt.links.new(fade.outputs["Result"], k.inputs[1])
    setin(b, ("Emission Color", "Emission"), srgb("c0102c"))
    nt.links.new(k.outputs[0], b.inputs["Emission Strength"])
    return m

def vinyl():
    # Black vinyl: its grooves as fine rings round the spindle (a bump), quite matt, so the turning picture has no
    # highlight baked into it that would turn with it (the page lays a still sheen over it instead).
    m = material("vinyl", srgb("070607"), rough=0.42, spec=0.35)
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    coords = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Location"].default_value = (-PLATTER[0], -PLATTER[1], 0)
    dist = nt.nodes.new("ShaderNodeVectorMath"); dist.operation = "LENGTH"
    wave = nt.nodes.new("ShaderNodeMath"); wave.operation = "SINE"
    sc = nt.nodes.new("ShaderNodeMath"); sc.operation = "MULTIPLY"; sc.inputs[1].default_value = 2600.0
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.25
    nt.links.new(coords.outputs["Object"], mp.inputs["Vector"])
    nt.links.new(mp.outputs["Vector"], dist.inputs[0])
    nt.links.new(dist.outputs["Value"], sc.inputs[0])
    nt.links.new(sc.outputs[0], wave.inputs[0])
    nt.links.new(wave.outputs[0], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

# ================================================================ layout (metres; the plinth's centre at 0, its top at z 0)
PLINTH = (0.453, 0.353, 0.045)
PLATTER = (-0.052, -0.004)         # the platter's centre
PLATTER_R = 0.166
RECORD_R = 0.1524                  # a 12" record
LABEL_R = 0.05
PIVOT = (0.158, 0.105)             # the tonearm's pivot
ARM = 0.230                        # pivot to stylus
GROOVE_START, GROOVE_END = 0.146, 0.060   # the needle's radius at the record's start and end

LACQUER = material("onyx lacquer", srgb("0a080b"), rough=0.22, coat=1.0, spec=0.5)
SATIN = material("satin black", srgb("0d0b0e"), rough=0.5, spec=0.4)
CHROME = material("black chrome", srgb("2a2629"), metallic=1.0, rough=0.18)
PLATTER_METAL = material("platter", srgb("1a171b"), metallic=1.0, rough=0.38)
DOTS = material("strobe dots", srgb("8a8084"), metallic=1.0, rough=0.25)
VINYL = vinyl()
LABEL = material("label", srgb("120c0e"), rough=0.6, spec=0.3)
NEON = neon("neon edge", 9.0)
NEON_SOFT = neon("neon soft", 4.0)
TARGET = emission("target light", srgb("ff5a2c"), 6.0 * LIT)
FLOOR = grid_floor()

def link(o):
    scene.collection.objects.link(o)
    return o

def assign(o, m):
    o.data.materials.clear()
    o.data.materials.append(m)

def box(name, size, loc, mat, bevel=0.0, segs=3):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)   # (all three are on by default)
    if bevel > 0:
        mod = o.modifiers.new("bevel", "BEVEL"); mod.width = bevel; mod.segments = segs
    assign(o, mat)
    return o

def cylinder(name, r, depth, loc, mat, verts=128):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, location=loc, vertices=verts)
    o = bpy.context.object
    o.name = name
    bpy.ops.object.shade_smooth()
    assign(o, mat)
    return o

def tube(name, pts, r, mat, closed=False):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = r
    cu.bevel_resolution = 4
    sp = cu.splines.new("POLY")
    sp.points.add(len(pts) - 1)
    for i, p in enumerate(pts):
        sp.points[i].co = (p[0], p[1], p[2], 1)
    sp.use_cyclic_u = closed
    o = link(bpy.data.objects.new(name, cu))
    o.data.materials.append(mat)
    return o

def ring(name, centre, r, z, thickness, mat, n=180):
    return tube(name, [(centre[0] + r * math.cos(2 * math.pi * k / n), centre[1] + r * math.sin(2 * math.pi * k / n), z) for k in range(n)],
                thickness, mat, closed=True)

def rounded_rect(cx, cy, w, h, r, n=8):
    pts = []
    for (qx, qy, a0) in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for k in range(n + 1):
            a = math.radians(a0 + 90 * k / n)
            pts.append((cx + qx + r * math.cos(a), cy + qy + r * math.sin(a)))
    return pts

groups = {"deck": [], "platter": [], "arm": []}
def put(group, o):
    groups[group].append(o)
    return o

# ---------------------------------------------------------------- the plinth
put("deck", box("plinth", PLINTH, (0, 0, -PLINTH[2] / 2), LACQUER, bevel=0.006))
put("deck", tube("plinth edge", [(x, y, -0.004) for x, y in rounded_rect(0, 0, PLINTH[0] + 0.0012, PLINTH[1] + 0.0012, 0.008)], 0.0012, NEON, closed=True))
# a top plate, a shade apart from the plinth, and the platter's well
put("deck", box("top plate", (PLINTH[0] - 0.02, PLINTH[1] - 0.02, 0.002), (0, 0, 0.001), SATIN, bevel=0.004))
put("deck", cylinder("platter well", PLATTER_R + 0.006, 0.004, (PLATTER[0], PLATTER[1], 0.001), CHROME))
# START / STOP at the front left, 33 / 45 at the front right (clear of the platter), a soft neon line round each
for name, (x, y, w, h) in { "start": (-0.192, -0.153, 0.044, 0.03), "33": (0.078, -0.156, 0.026, 0.016), "45": (0.110, -0.156, 0.026, 0.016) }.items():
    put("deck", box("button " + name, (w, h, 0.006), (x, y, 0.004), CHROME, bevel=0.002))
    put("deck", tube("button edge " + name, [(px, py, 0.0075) for px, py in rounded_rect(x, y, w, h, 0.003)], 0.0006, NEON_SOFT, closed=True))
# the target light at the front left corner, glowing
put("deck", cylinder("target light base", 0.012, 0.012, (-0.205, 0.15, 0.006), CHROME, verts=48))
put("deck", cylinder("target light", 0.006, 0.004, (-0.205, 0.15, 0.0135), TARGET, verts=32))
# the pitch fader down the right, its slot lined with neon, its cap
FADER = (0.195, -0.075)
put("deck", box("fader slot", (0.006, 0.11, 0.004), (FADER[0], FADER[1], 0.001), material("slot", srgb("020102"), rough=0.6)))
for side in (-1, 1):
    put("deck", tube("fader edge", [(FADER[0] + side * 0.005, FADER[1] - 0.055, 0.003), (FADER[0] + side * 0.005, FADER[1] + 0.055, 0.003)], 0.0006, NEON_SOFT))
put("deck", box("fader cap", (0.026, 0.016, 0.014), (FADER[0], FADER[1], 0.009), CHROME, bevel=0.003))
put("deck", tube("fader cap line", [(FADER[0] - 0.011, FADER[1], 0.0165), (FADER[0] + 0.011, FADER[1], 0.0165)], 0.0006, NEON))
# the tonearm's base: a round plinth, the pivot's gimbal and the arm rest
put("deck", cylinder("arm base", 0.034, 0.008, (PIVOT[0], PIVOT[1], 0.004), CHROME, verts=96))
put("deck", ring("arm base ring", PIVOT, 0.0345, 0.0085, 0.0008, NEON_SOFT))
put("deck", cylinder("arm rest", 0.004, 0.03, (PIVOT[0] + 0.01, PIVOT[1] - 0.105, 0.015), CHROME, verts=24))

# ---------------------------------------------------------------- the platter and record (turned by the page)
put("platter", cylinder("platter", PLATTER_R, 0.016, (PLATTER[0], PLATTER[1], 0.012), PLATTER_METAL, verts=192))
put("platter", ring("platter rim neon", PLATTER, PLATTER_R + 0.0002, 0.0195, 0.0011, NEON))
for k in range(96):   # the strobe dots round the rim's top edge
    a = 2 * math.pi * k / 96
    put("platter", cylinder("dot %d" % k, 0.0016, 0.001, (PLATTER[0] + (PLATTER_R - 0.0055) * math.cos(a), PLATTER[1] + (PLATTER_R - 0.0055) * math.sin(a), 0.0203),
                             DOTS, verts=12))
put("platter", cylinder("slipmat", RECORD_R + 0.002, 0.002, (PLATTER[0], PLATTER[1], 0.021), SATIN, verts=192))
put("platter", cylinder("record", RECORD_R, 0.0018, (PLATTER[0], PLATTER[1], 0.0229), VINYL, verts=256))
put("platter", cylinder("label", LABEL_R, 0.0004, (PLATTER[0], PLATTER[1], 0.0240), LABEL, verts=128))
put("platter", ring("label ring", PLATTER, LABEL_R - 0.002, 0.0244, 0.0008, NEON))
put("platter", ring("label inner ring", PLATTER, LABEL_R * 0.55, 0.0244, 0.0004, NEON_SOFT))
# PR7-T on the label, in the plugin's own lettering, soft neon; a mark across the label shows the record turning
font = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "fonts", "InterDisplay-BlackItalic.ttf")
bpy.ops.object.text_add(location=(PLATTER[0], PLATTER[1] - LABEL_R * 0.5, 0.0246))
name = bpy.context.object
name.data.body = "PR7-T"
name.data.align_x = "CENTER"
name.data.align_y = "CENTER"
name.data.size = 0.017
if os.path.exists(font):
    name.data.font = bpy.data.fonts.load(font)
name.data.materials.append(NEON_SOFT)
put("platter", name)
put("platter", tube("label mark", [(PLATTER[0], PLATTER[1] + LABEL_R * 0.2, 0.0246), (PLATTER[0], PLATTER[1] + LABEL_R * 0.86, 0.0246)], 0.0012, NEON))
put("platter", cylinder("spindle", 0.0036, 0.012, (PLATTER[0], PLATTER[1], 0.027), CHROME, verts=32))

# ---------------------------------------------------------------- the tonearm (swung by the page), its stylus at the record's start
def needle_at(r):
    # Where a circle of radius r round the platter's centre meets the arm's reach from the pivot (the inner meeting).
    c, p = Vector((PLATTER[0], PLATTER[1])), Vector(PIVOT)
    d = (p - c).length
    a = (d * d - ARM * ARM + r * r) / (2 * d)
    h = math.sqrt(max(0.0, r * r - a * a))
    u = (p - c) / d
    mid = c + u * a
    return mid + Vector((u.y, -u.x)) * h

STYLUS = needle_at(GROOVE_START)
along = (STYLUS - Vector(PIVOT)).normalized()
across = Vector((-along.y, along.x))
z_arm = 0.045
put("arm", cylinder("pivot", 0.018, 0.02, (PIVOT[0], PIVOT[1], 0.022), CHROME, verts=64))
put("arm", ring("pivot ring", PIVOT, 0.0185, 0.032, 0.0008, NEON))
back = Vector(PIVOT) - along * 0.055   # the counterweight behind the pivot
put("arm", cylinder("counterweight", 0.016, 0.03, (back.x, back.y, z_arm), CHROME, verts=48)).rotation_euler = (0, math.pi / 2, math.atan2(along.y, along.x))
end = STYLUS - along * 0.03
put("arm", tube("arm tube", [(back.x, back.y, z_arm), (PIVOT[0], PIVOT[1], z_arm), (end.x, end.y, z_arm)], 0.0042, CHROME))
put("arm", tube("arm neon", [(PIVOT[0] + along.x * 0.02, PIVOT[1] + along.y * 0.02, z_arm + 0.0043), (end.x, end.y, z_arm + 0.0043)], 0.0009, NEON))
# the headshell, turned a little in (as a real one is offset), its edge in neon, the cartridge under it
offset = math.radians(22)
hs_dir = Vector((along.x * math.cos(offset) - along.y * math.sin(offset), along.x * math.sin(offset) + along.y * math.cos(offset)))
hs_across = Vector((-hs_dir.y, hs_dir.x))
hs_c = STYLUS - hs_dir * 0.012
corners = [hs_c + hs_dir * dx + hs_across * dy for dx, dy in ((0.024, 0.009), (-0.024, 0.009), (-0.024, -0.009), (0.024, -0.009))]
put("arm", box("headshell", (0.05, 0.019, 0.006), (hs_c.x, hs_c.y, z_arm - 0.004), CHROME, bevel=0.002)).rotation_euler = (0, 0, math.atan2(hs_dir.y, hs_dir.x))
put("arm", tube("headshell edge", [(c.x, c.y, z_arm - 0.0005) for c in corners], 0.0007, NEON, closed=True))
put("arm", tube("finger lift", [(hs_c.x + hs_dir.x * 0.024 + hs_across.x * 0.008, hs_c.y + hs_dir.y * 0.024 + hs_across.y * 0.008, z_arm - 0.003),
                                (hs_c.x + hs_dir.x * 0.03 + hs_across.x * 0.02, hs_c.y + hs_dir.y * 0.03 + hs_across.y * 0.02, z_arm - 0.003)], 0.0014, CHROME))

# ---------------------------------------------------------------- floor, lights, camera
floor = box("floor", (2.0, 2.0, 0.001), (0, 0, -PLINTH[2] - 0.0005), FLOOR)
put("deck", floor)

world = bpy.data.worlds.new("dark")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = srgb("060407")
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.004

def light(name, kind, loc, rot, energy, colour, size=1.0, size_y=None):
    data = bpy.data.lights.new(name, kind)
    data.energy = energy
    data.color = colour[:3]
    if kind == "AREA":
        data.size = size
        if size_y:
            data.shape = "RECTANGLE"
            data.size_y = size_y
    o = link(bpy.data.objects.new(name, data))
    o.location = loc
    o.rotation_euler = rot
    return o

LIGHT = float(os.environ.get("DECK_LIGHT", "0.2"))
light("key", "AREA", (-0.3, 0.4, 0.9), (math.radians(-25), math.radians(-20), 0), 1.4 * LIGHT, (1.0, 0.96, 0.95), size=0.6)
light("sheen", "AREA", (0.15, 0.3, 1.0), (math.radians(-14), math.radians(10), 0), 1.6 * LIGHT, (1.0, 0.95, 0.96), size=0.3, size_y=0.8)
light("rim pink", "AREA", (-0.45, 0.0, 0.12), (0, math.radians(-75), 0), 3.6 * LIGHT * 2, srgb("ff3b6a"), size=0.12, size_y=0.9)
light("rim orange", "AREA", (0.45, 0.0, 0.12), (0, math.radians(75), 0), 3.6 * LIGHT * 2, srgb("ff5a2c"), size=0.12, size_y=0.9)

cam_data = bpy.data.cameras.new("camera")
cam_data.type = "ORTHO"
PX_PER_M = 2500.0 * WIDTH / 1400.0          # 2500 px a metre at the plugin's 2x picture (1400 x 1080)
cam_data.ortho_scale = WIDTH / PX_PER_M
cam_data.clip_end = 10
cam = link(bpy.data.objects.new("camera", cam_data))
cam.location = (0.0, 0.0, 1.5)              # straight down over the plinth's centre
cam.rotation_euler = (0, 0, 0)
scene.camera = cam

# What each pass shows.
if PASS in ("platter", "arm"):
    keep = set(groups[PASS])
    for o in scene.objects:
        if o.type in ("MESH", "CURVE", "FONT") and o not in keep:
            o.hide_render = True
    scene.render.film_transparent = True
else:
    for o in groups["arm"]:
        o.hide_render = True

scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = SAMPLES
scene.cycles.use_denoising = False
scene.render.resolution_x = WIDTH
scene.render.resolution_y = HEIGHT
scene.render.image_settings.color_mode = "RGBA" if PASS in ("platter", "arm") else "RGB"
enum = [v.identifier for v in scene.view_settings.bl_rna.properties["view_transform"].enum_items]
scene.view_settings.view_transform = "Filmic" if "Filmic" in enum else "Standard"
scene.cycles.seed = 7
scene.use_nodes = True
tree = scene.node_tree
tree.nodes.clear()
layers = tree.nodes.new("CompositorNodeRLayers")
glare = tree.nodes.new("CompositorNodeGlare")
glare.glare_type = "FOG_GLOW"
glare.quality = "HIGH"
glare.threshold = 0.9
glare.size = 8
glare.mix = -0.35
comp = tree.nodes.new("CompositorNodeComposite")
tree.links.new(layers.outputs["Image"], glare.inputs["Image"])
tree.links.new(glare.outputs["Image"], comp.inputs["Image"])
tree.links.new(layers.outputs["Alpha"], comp.inputs["Alpha"])

# Where things land in the picture (px from its top left), for the page.
if GEOMETRY:
    bpy.context.view_layer.update()
    def px(x, y, z=0.0):
        v = world_to_camera_view(scene, cam, Vector((x, y, z)))
        return [round(v.x * WIDTH, 2), round((1 - v.y) * HEIGHT, 2)]
    geo = {"width": WIDTH, "height": HEIGHT, "pxPerMetre": PX_PER_M,
           "platterCentre": px(*PLATTER), "platterRadius": PLATTER_R * PX_PER_M, "recordRadius": RECORD_R * PX_PER_M,
           "pivot": px(*PIVOT), "arm": ARM * PX_PER_M, "stylus": px(STYLUS.x, STYLUS.y),
           "grooveStart": GROOVE_START * PX_PER_M, "grooveEnd": GROOVE_END * PX_PER_M}
    with open(GEOMETRY, "w") as f:
        json.dump(geo, f, indent=1)

scene.render.filepath = OUT
bpy.ops.render.render(write_still=True)
print("RENDERED", OUT)
