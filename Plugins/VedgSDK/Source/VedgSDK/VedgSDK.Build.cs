// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VedgSDK : ModuleRules
{
	public VedgSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[] { "EngineSettings" });
	}
}
