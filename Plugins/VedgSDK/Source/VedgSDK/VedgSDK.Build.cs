// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VedgSDK : ModuleRules
{
	public VedgSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Several VedgSDK translation units define file-local helpers with identical names in anonymous namespaces.
		bUseUnity = false;

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

		PrivateDependencyModuleNames.AddRange(new string[] { "EngineSettings", "WebSockets", "UMG", "Slate", "SlateCore", "Projects" });

		// HMAC-SHA256 for the login attestation (VedgAttestation).
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		// shortcut: UBT doesn't track env vars, so after changing VEDG_BUILD_KEY force a rebuild of this module.
		string BuildKey = (System.Environment.GetEnvironmentVariable("VEDG_BUILD_KEY") ?? "").Trim();
		PrivateDefinitions.Add("VEDG_BUILD_KEY=\"" + BuildKey + "\"");

		// Panel/error strings, read at runtime by FVedgText (same CSVs as the Godot addon).
		RuntimeDependencies.Add("$(PluginDir)/Resources/Translations/*.csv", StagedFileType.UFS);
	}
}
