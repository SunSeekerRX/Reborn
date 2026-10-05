using UnrealBuildTool;
public class HorrorSystems : ModuleRules
{
    public HorrorSystems(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "DeveloperSettings", "AIModule",
            "NavigationSystem", "GameplayTasks", "AnimGraphRuntime"
        });
        if(Target.bBuildEditor) PrivateDependencyModuleNames.Add("AssetRegistry");
        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "ApplicationCore", "AnimationCore" });
    }
}
