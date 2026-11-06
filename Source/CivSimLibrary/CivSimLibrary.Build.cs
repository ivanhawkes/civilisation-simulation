using UnrealBuildTool;
using System.IO;

public class CivSimLibrary : ModuleRules
{
    public CivSimLibrary(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "CivSimCore", "Public"));

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "YamlCpp"
            }
        );
    }
}
