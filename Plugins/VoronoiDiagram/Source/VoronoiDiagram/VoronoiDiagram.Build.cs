// Copyright 2015 afuzzyllama. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class VoronoiDiagram : ModuleRules
{
    public VoronoiDiagram(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        
        PrivateIncludePaths.AddRange(
            new string[] {
                "VoronoiDiagram/Private"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );
    }
}
