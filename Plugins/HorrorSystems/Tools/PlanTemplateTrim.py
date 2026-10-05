"""Find the dependency closure of the mannequin + locomotion blendspace; no deletion here."""
import unreal as u
from pathlib import Path
import json
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/HorrorSystems"],force_rescan=True)
options=u.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
roots=["/HorrorSystems/Characters/Mannequins/Meshes/SKM_Manny_Simple","/HorrorSystems/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"]
keep=set(); pending=roots[:]
while pending:
    path=pending.pop()
    if path in keep: continue
    keep.add(path)
    for dependency in registry.get_dependencies(path,options):
        dep=str(dependency)
        if dep.startswith("/HorrorSystems/Characters/") and dep not in keep: pending.append(dep)
all_paths={p.split(".")[0] for p in u.EditorAssetLibrary.list_assets("/HorrorSystems/Characters",True,False)}
report={"keep":sorted(keep),"backup":sorted(all_paths-keep)}
target=Path(u.Paths.project_saved_dir())/"Verification/TemplateTrim.json"
target.write_text(json.dumps(report,indent=2),encoding="utf-8")
u.log("HS_TEMPLATE_TRIM_PLAN kept="+str(len(keep))+" unused="+str(len(all_paths-keep)))
