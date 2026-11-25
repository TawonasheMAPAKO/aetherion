"""
Aetherion: turns the open level into Serrath's Scar.

Open Variant_Combat/Lvl_Combat first, then:
  Tools > Execute Python Script... > Scripts/setup_scar_look.py

It changes only the open level and two new materials. Everything is in git, so
`git checkout -- Content` (or the commit before this one) brings the old look back.

  1. Builds M_ScarRock (dark basalt with glowing cracks) and M_ScarLava (flowing lava)
  2. Re-skins every static mesh: lava stays lava, everything else becomes basalt
  3. Low ember sun with light shafts, ashen sky, dim red-violet sky light
  4. Thick height fog with volumetric fog so light rays show in the haze
  5. A post-process grade: crushed shadows, warm highlights, bloom, vignette, grain
  6. Adds an ember field drifting up through the arena
"""
import unreal

FX = "/Game/Aetherion/FX"
ROCK = FX + "/M_ScarRock"
LAVA = FX + "/M_ScarLava"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
problems = []


def log(msg):
    unreal.log("[Aetherion] " + msg)


def warn(msg):
    problems.append(msg)
    unreal.log_warning("[Aetherion] " + msg)


def setp(obj, name, value):
    """Set a property, tolerating names that differ between engine versions."""
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as e:
        warn("Skipped %s.%s (%s)" % (obj.get_class().get_name(), name, e))
        return False


# ------------------------------------------------------------------ materials
def fresh_material(path):
    folder, name = path.rsplit("/", 1)
    if not eal.does_directory_exist(folder):
        eal.make_directory(folder)
    if eal.does_asset_exist(path):
        m = eal.load_asset(path)
        mel.delete_all_material_expressions(m)
        return m
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def node(m, cls, x, y, **props):
    n = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        setp(n, k, v)
    return n


def link(a, b, pin=""):
    mel.connect_material_expressions(a, "", b, pin)


def mul(m, a, b, x, y):
    n = node(m, unreal.MaterialExpressionMultiply, x, y)
    link(a, n, "A")
    link(b, n, "B")
    return n


def color(m, rgb, x, y):
    return node(m, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def scalar(m, v, x, y):
    return node(m, unreal.MaterialExpressionConstant, x, y, r=v)


def noise(m, scale, levels, turbulence, out_min, out_max, x, y):
    return node(m, unreal.MaterialExpressionNoise, x, y, scale=scale, levels=levels,
                turbulence=turbulence, output_min=out_min, output_max=out_max, quality=2)


def build_rock():
    m = fresh_material(ROCK)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1800, 0)

    # basalt: two dark tones broken up by large-scale noise
    big = noise(m, 0.0035, 6, True, 0.0, 1.0, -1500, -200)
    link(wp, big, "Position")
    lerp = node(m, unreal.MaterialExpressionLinearInterpolate, -1000, -250)
    link(color(m, (0.045, 0.036, 0.034), -1300, -400), lerp, "A")
    link(color(m, (0.14, 0.11, 0.1), -1300, -300), lerp, "B")
    link(big, lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = node(m, unreal.MaterialExpressionLinearInterpolate, -1000, -50)
    link(scalar(m, 0.92, -1300, -100), rough, "A")
    link(scalar(m, 0.6, -1300, -20), rough, "B")
    link(big, rough, "Alpha")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # glowing cracks: thin ridges where signed noise crosses zero
    fine = noise(m, 0.011, 4, False, -1.0, 1.0, -1500, 200)
    link(wp, fine, "Position")
    ab = node(m, unreal.MaterialExpressionAbs, -1250, 200)
    link(fine, ab)
    inv = node(m, unreal.MaterialExpressionOneMinus, -1100, 200)
    link(ab, inv)
    c2 = mul(m, inv, inv, -950, 200)
    c4 = mul(m, c2, c2, -820, 200)
    c8 = mul(m, c4, c4, -690, 200)
    c16 = mul(m, c8, c8, -560, 200)
    hot = mul(m, color(m, (1.0, 0.22, 0.03), -700, 350), scalar(m, 9.0, -700, 430), -560, 350)
    glow = mul(m, c16, hot, -400, 250)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(m)
    eal.save_asset(ROCK, only_if_is_dirty=False)
    return m


def build_lava():
    m = fresh_material(LAVA)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -1900, 0)
    t = node(m, unreal.MaterialExpressionTime, -1900, 150)
    flow = mul(m, t, color(m, (35.0, 12.0, 6.0), -1900, 250), -1700, 200)
    pos = node(m, unreal.MaterialExpressionAdd, -1550, 100)
    link(wp, pos, "A")
    link(flow, pos, "B")

    n = noise(m, 0.006, 5, True, 0.0, 1.0, -1350, 100)
    link(pos, n, "Position")
    h2 = mul(m, n, n, -1150, 100)
    h3 = mul(m, h2, n, -1000, 100)

    mel.connect_material_property(color(m, (0.03, 0.012, 0.01), -800, -150), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(m, 0.55, -800, -60), "", unreal.MaterialProperty.MP_ROUGHNESS)

    heat = node(m, unreal.MaterialExpressionAdd, -850, 100)
    link(mul(m, h3, scalar(m, 60.0, -1000, 220), -900, 180), heat, "A")
    link(scalar(m, 1.2, -1000, 300), heat, "B")
    glow = mul(m, color(m, (1.0, 0.28, 0.035), -850, 300), heat, -650, 150)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(m)
    eal.save_asset(LAVA, only_if_is_dirty=False)
    return m


try:
    rock_mat = build_rock()
    lava_mat = build_lava()
    log("Built M_ScarRock and M_ScarLava")
except Exception as e:
    rock_mat = lava_mat = None
    warn("Could not build the Scar materials (%s)" % e)

# ------------------------------------------------------------------ re-skin the level
all_actors = actors_sub.get_all_level_actors()


def is_lava(comp, actor):
    if "lava" in actor.get_class().get_name().lower() or "lava" in actor.get_actor_label().lower():
        return True
    try:
        for i in range(comp.get_num_materials()):
            mat = comp.get_material(i)
            if mat and "lava" in mat.get_name().lower():
                return True
    except Exception:
        pass
    return False


skinned = lava_count = 0
if rock_mat and lava_mat:
    for actor in all_actors:
        label = actor.get_actor_label().lower()
        if "sky" in label or "cloud" in label:
            continue
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.get_class().get_name() == "InstancedStaticMeshComponent" and "ember" in label:
                continue
            target = lava_mat if is_lava(comp, actor) else rock_mat
            try:
                # mark the actor as edited, or the save skips it and the change is lost on reopen
                actor.modify()
                comp.modify()
                for i in range(comp.get_num_materials()):
                    comp.set_material(i, target)
                skinned += 1
                if target == lava_mat:
                    lava_count += 1
            except Exception as e:
                warn("Could not re-skin %s (%s)" % (actor.get_actor_label(), e))
    log("Re-skinned %d meshes (%d as lava)" % (skinned, lava_count))


# ------------------------------------------------------------------ lighting & atmosphere
def first(cls):
    for a in all_actors:
        if isinstance(a, cls):
            return a
    return None


def ensure(cls, location=unreal.Vector(0, 0, 0)):
    a = first(cls)
    if a is None:
        a = actors_sub.spawn_actor_from_class(cls, location)
        log("Added " + cls.__name__)
    return a


# low ember sun
sun = ensure(unreal.DirectionalLight)
if sun:
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-24.0, yaw=-35.0), False)
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if lc:
        setp(lc, "intensity", 12.0)
        setp(lc, "light_color", unreal.Color(r=255, g=128, b=72, a=255))
        setp(lc, "enable_light_shaft_bloom", True)
        setp(lc, "bloom_scale", 0.4)
        setp(lc, "bloom_threshold", 0.1)
        setp(lc, "bloom_tint", unreal.Color(r=255, g=150, b=90, a=255))
        setp(lc, "atmosphere_sun_light", True)
    log("Set a low ember sun")

# ashen, smoky sky
sky = first(unreal.SkyAtmosphere)
if sky:
    sc = sky.get_component_by_class(unreal.SkyAtmosphereComponent)
    if sc:
        setp(sc, "mie_scattering_scale", 0.02)
        setp(sc, "mie_absorption_scale", 0.01)
        setp(sc, "mie_scattering", unreal.LinearColor(0.75, 0.55, 0.5, 1.0))
        setp(sc, "rayleigh_scattering", unreal.LinearColor(0.12, 0.09, 0.11, 1.0))
        setp(sc, "multi_scattering_factor", 0.6)
    log("Darkened the sky")

skylight = first(unreal.SkyLight)
if skylight:
    slc = skylight.get_component_by_class(unreal.SkyLightComponent)
    if slc:
        setp(slc, "intensity", 1.4)
        setp(slc, "light_color", unreal.Color(r=150, g=120, b=170, a=255))
        setp(slc, "lower_hemisphere_color", unreal.LinearColor(0.08, 0.02, 0.01, 1.0))
        try:
            slc.recapture_sky()
        except Exception:
            pass

# fog and volumetric haze
fog = ensure(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200))
if fog:
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fc:
        setp(fc, "fog_density", 0.02)
        setp(fc, "fog_height_falloff", 0.35)
        if not setp(fc, "fog_inscattering_luminance", unreal.LinearColor(0.16, 0.05, 0.03, 1.0)):
            setp(fc, "fog_inscattering_color", unreal.LinearColor(0.16, 0.05, 0.03, 1.0))
        setp(fc, "start_distance", 300.0)
        setp(fc, "volumetric_fog", True)
        setp(fc, "volumetric_fog_scattering_distribution", 0.75)
        setp(fc, "volumetric_fog_albedo", unreal.Color(r=255, g=190, b=170, a=255))
        setp(fc, "volumetric_fog_extinction_scale", 1.0)
    log("Added smoky height fog with volumetric light")

# ------------------------------------------------------------------ colour grade
ppv = None
for a in all_actors:
    if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label().startswith("Aetherion"):
        ppv = a
if ppv is None:
    ppv = actors_sub.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    ppv.set_actor_label("Aetherion_ScarGrade")
setp(ppv, "unbound", True)
setp(ppv, "priority", 10.0)

pp = ppv.get_editor_property("settings")
grade = {
    "bloom_intensity": 1.1,
    "bloom_threshold": 0.8,
    "vignette_intensity": 0.4,
    "film_grain_intensity": 0.12,
    "scene_fringe_intensity": 0.6,
    "auto_exposure_bias": 0.6,
    "color_saturation": unreal.Vector4(1.0, 0.95, 0.92, 0.88),
    "color_contrast": unreal.Vector4(1.0, 1.0, 1.0, 1.15),
    "color_gamma_shadows": unreal.Vector4(0.96, 0.94, 1.04, 0.92),
    "color_gain_highlights": unreal.Vector4(1.08, 0.98, 0.9, 1.05),
    "white_temp": 5600.0,
}
for key, value in grade.items():
    try:
        pp.set_editor_property("override_" + key, True)
        pp.set_editor_property(key, value)
    except Exception as e:
        warn("Skipped grade setting %s (%s)" % (key, e))
setp(ppv, "settings", pp)
log("Applied the Scar colour grade")

# ------------------------------------------------------------------ embers
ember_cls = unreal.load_class(None, "/Script/Aetherion.EmberField")
if ember_cls is None:
    warn("EmberField class not found. Rebuild the C++ first, then run this again.")
else:
    if not any(a.get_class() == ember_cls for a in all_actors):
        start = first(unreal.PlayerStart)
        loc = start.get_actor_location() if start else unreal.Vector(0, 0, 0)
        loc.z += 400
        e = actors_sub.spawn_actor_from_class(ember_cls, loc)
        e.set_actor_label("Aetherion_Embers")
        e.modify()
        log("Added drifting embers")

# ------------------------------------------------------------------ save
saved = False
try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    saved = True
except Exception as e:
    warn("Level save step failed (%s)" % e)
try:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    saved = True
except Exception as e:
    warn("Could not save automatically (%s). Use File > Save All." % e)
if saved:
    log("Saved the level and materials")

if problems:
    unreal.log_warning("[Aetherion] Done with %d skipped step(s); the rest was applied." % len(problems))
else:
    log("Done. Press Play.")
