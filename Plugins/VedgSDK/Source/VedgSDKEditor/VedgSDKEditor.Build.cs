// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VedgSDKEditor : ModuleRules
{
	public VedgSDKEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"InputCore",
			"Json",
			"Slate",
			"SlateCore",
			"Projects",
			"UnrealEd",
			"WorkspaceMenuStructure",
			"VedgSDK",
		});
	}
}
