using System.IO;
using UnrealBuildTool;

public class EngineMaterialLab : ModuleRules {
    public EngineMaterialLab(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true; // Standalone UPC currently throws std::exceptions.
        PrivateDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine"});
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty", "include"));
    }
}
