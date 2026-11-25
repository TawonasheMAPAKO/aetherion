"""
Aetherion: prepares Paragon Sun Wukong as the Shard Warden.

Run once in the Unreal editor after the C++ has compiled:
  Tools > Execute Python Script... > Scripts/setup_hero.py

Creates two montages the Warden uses for Inferno Cleave:
  /Game/Aetherion/Hero/AM_Wukong_Cleave_I    (from Primary_Melee_E_Slow, a heavy staff strike)
  /Game/Aetherion/Hero/AM_Wukong_Cleave_III  (from Q_Slam, the stage III ground slam)
  /Game/Aetherion/Hero/AM_Wukong_Dodge_Fwd / _Bwd  (front and back flips for the dodge)
The model, animation blueprint and basic combo are set in C++ (BP_Warden > Hero).
"""
import unreal

ANIMS = "/Game/ParagonSunWukong/Characters/Heroes/Wukong/Animations/"
OUT = "/Game/Aetherion/Hero"
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
problems = []


def log(msg):
    unreal.log("[Aetherion] " + msg)


def make_montage(name, source):
    path = OUT + "/" + name
    if eal.does_asset_exist(path):
        log(name + " already exists")
        return
    seq = eal.load_asset(ANIMS + source)
    if seq is None:
        problems.append("Missing animation " + source)
        unreal.log_warning("[Aetherion] Missing animation " + source)
        return
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", seq)
    try:
        factory.set_editor_property("target_skeleton", seq.get_editor_property("skeleton"))
    except Exception:
        pass
    montage = tools.create_asset(name, OUT, unreal.AnimMontage, factory)
    if montage:
        eal.save_asset(path, only_if_is_dirty=False)
        log("Created " + name)
    else:
        problems.append("Could not create " + name)


if not eal.does_directory_exist(OUT):
    eal.make_directory(OUT)

make_montage("AM_Wukong_Cleave_I", "Primary_Melee_E_Slow")
make_montage("AM_Wukong_Cleave_III", "Q_Slam")
make_montage("AM_Wukong_Dodge_Fwd", "Q_Flip_Fwd")
make_montage("AM_Wukong_Dodge_Bwd", "Q_Flip_Bwd")

if problems:
    unreal.log_warning("[Aetherion] Finished with problems: " + "; ".join(problems))
else:
    log("Done. Press Play in Lvl_Combat.")
