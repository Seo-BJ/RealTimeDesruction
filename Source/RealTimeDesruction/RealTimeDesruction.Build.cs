// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RealTimeDesruction : ModuleRules
{
	public RealTimeDesruction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent",
			"GeometryAlgorithms",
			"Eigen"




		});

       
    }
}
