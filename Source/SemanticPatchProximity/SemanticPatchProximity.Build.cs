using UnrealBuildTool;

public class SemanticPatchProximity : ModuleRules
{
    public SemanticPatchProximity(ReadOnlyTargetRules target) : base(target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "SemanticSurfacePatches"
            });
    }
}
