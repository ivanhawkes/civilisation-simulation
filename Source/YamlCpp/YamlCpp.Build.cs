using System.IO;
using UnrealBuildTool;

public class YamlCpp : ModuleRules
{
    public YamlCpp(ReadOnlyTargetRules Target) : base(Target)
    {
        // yaml-cpp uses C++ exceptions internally. Keep this setting isolated
        // to this module rather than enabling exceptions for the whole plugin.
        bEnableExceptions = true;

        string YamlRoot = Path.Combine(
            ModuleDirectory,
            "Private",
            "ThirdParty",
            "yaml-cpp"
        );

        string YamlInclude = Path.Combine(YamlRoot, "include");

        if (!Directory.Exists(YamlInclude))
        {
            throw new BuildException(
                "yaml-cpp is missing. Run Scripts/FetchYamlCpp.sh " +
                "from the CivSimLibrary plugin source directory."
            );
        }

        PublicSystemIncludePaths.Add(YamlInclude);
        PublicDefinitions.Add("YAML_CPP_STATIC_DEFINE");

        PrivateDependencyModuleNames.Add("Core");
    }
}
