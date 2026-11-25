"""
Aetherion: brightens Serrath's Scar after setup_scar_look.py came out too dark.

Open Variant_Combat/Lvl_Combat, then:
  Tools > Execute Python Script... > Scripts/tune_scar_lighting.py

Adjust BRIGHTNESS below and run it again to taste. Safe to re-run.
  - Sun higher in the sky and about 3x stronger
  - Sky light and fog lifted so shadows are not black
  - Camera exposure no longer locked: it adapts again, with a gentle bias
  - Basalt rock a little lighter so it reads in the dark
"""
import unreal

BRIGHTNESS = 1.0   # 0.7 = moodier, 1.3 = brighter

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actors_sub.get_all_level_actors()


def log(m):
    unreal.log("[Aetherion] " + m)


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:
        unreal.log_warning("[Aetherion] Skipped %s (%s)" % (name, e))


def first(cls):
    return next((a for a in actors if isinstance(a, cls)), None)


sun = first(unreal.DirectionalLight)
if sun:
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=-24.0, yaw=-35.0), False)
    lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    if lc:
        setp(lc, "intensity", 12.0 * BRIGHTNESS)
        setp(lc, "light_color", unreal.Color(r=255, g=150, b=95, a=255))
    log("Sun raised and strengthened")

sky = first(unreal.SkyLight)
if sky:
    slc = sky.get_component_by_class(unreal.SkyLightComponent)
    if slc:
        setp(slc, "intensity", 1.4 * BRIGHTNESS)
        setp(slc, "light_color", unreal.Color(r=190, g=165, b=200, a=255))
        try:
            slc.recapture_sky()
        except Exception:
            pass
    log("Sky light lifted")

fog = first(unreal.ExponentialHeightFog)
if fog:
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    if fc:
        setp(fc, "fog_density", 0.02)
        setp(fc, "volumetric_fog_extinction_scale", 1.0)
    log("Fog thinned")

for a in actors:
    if isinstance(a, unreal.PostProcessVolume) and a.get_actor_label().startswith("Aetherion"):
        pp = a.get_editor_property("settings")
        for key in ("auto_exposure_min_brightness", "auto_exposure_max_brightness"):
            try:
                pp.set_editor_property("override_" + key, False)
            except Exception:
                pass
        try:
            pp.set_editor_property("override_auto_exposure_bias", True)
            pp.set_editor_property("auto_exposure_bias", 0.6 * BRIGHTNESS)
            pp.set_editor_property("override_vignette_intensity", True)
            pp.set_editor_property("vignette_intensity", 0.4)
        except Exception as e:
            unreal.log_warning("[Aetherion] Skipped exposure (%s)" % e)
        a.set_editor_property("settings", pp)
        log("Exposure unlocked")

# lighter basalt: bump the two base colour constants in M_ScarRock
rock = eal.load_asset("/Game/Aetherion/FX/M_ScarRock")
if rock:
    try:
        for expr in mel.get_material_expressions(rock) if hasattr(mel, "get_material_expressions") else []:
            if isinstance(expr, unreal.MaterialExpressionConstant3Vector):
                c = expr.get_editor_property("constant")
                if c.r < 0.03 and c.g < 0.03:
                    expr.set_editor_property("constant", unreal.LinearColor(0.045, 0.036, 0.034, 1.0))
                elif c.r < 0.1 and c.g < 0.07:
                    expr.set_editor_property("constant", unreal.LinearColor(0.14, 0.11, 0.1, 1.0))
        mel.recompile_material(rock)
        eal.save_asset("/Game/Aetherion/FX/M_ScarRock", only_if_is_dirty=False)
        log("Rock lightened")
    except Exception as e:
        unreal.log_warning("[Aetherion] Could not lighten rock (%s)" % e)

try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
except Exception:
    pass
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Done. Press Play. Change BRIGHTNESS at the top of this script and re-run to adjust.")
