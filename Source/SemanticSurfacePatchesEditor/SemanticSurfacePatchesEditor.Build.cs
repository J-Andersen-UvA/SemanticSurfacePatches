using UnrealBuildTool;

public class SemanticSurfacePatchesEditor : ModuleRules
{
    public SemanticSurfacePatchesEditor(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "AnimationBlueprintLibrary",
                "Core",
                "CoreUObject",
                "Engine",
                "SemanticPatchProximity"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AssetRegistry",
                "DesktopPlatform",
                "InputCore",
                "Projects",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "UnrealEd",
                "Json",
                "Kismet",
                "SemanticSurfacePatches"
            });
    }
}
