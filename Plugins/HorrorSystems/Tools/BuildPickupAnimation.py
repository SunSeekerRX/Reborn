import unreal as u
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/HorrorSystems"],force_rescan=True)
assert u.HSAuthoringLibrary.create_pickup_animation_assets(),"Pickup animation generation failed"
assert u.EditorAssetLibrary.load_asset("/HorrorSystems/Characters/Interaction/AM_FirstPersonPickup")
u.log("HS_PICKUP_ANIMATION_SUCCESS")
