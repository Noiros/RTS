// Fill out your copyright notice in the Description page of Project Settings.

using System.Collections.Generic;
using System.IO;
using System.Text;
using Microsoft.Extensions.Logging;
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

		string ProjectDir = Target.ProjectFile != null ? Target.ProjectFile.Directory.FullName : null;

		// Build key: VEDG_BUILD_KEY, else the key the editor's Vedg tab caches after login (Saved/, never committed).
		// shortcut: UBT doesn't track env vars, so after changing VEDG_BUILD_KEY force a rebuild of this module.
		string BuildKey = (System.Environment.GetEnvironmentVariable("VEDG_BUILD_KEY") ?? "").Trim();
		if (BuildKey == "" && ProjectDir != null)
		{
			string KeyFile = Path.Combine(ProjectDir, "Saved", "VedgSDK", "build_key");
			ExternalDependencies.Add(KeyFile);
			if (File.Exists(KeyFile))
			{
				BuildKey = File.ReadAllText(KeyFile).Trim();
			}
		}
		if (BuildKey == "" && Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			Logger.LogWarning("VedgSDK: no build key (env VEDG_BUILD_KEY or log in from the editor's Vedg tab) - this build will be refused online");
		}
		PrivateDefinitions.Add("VEDG_BUILD_KEY=\"" + BuildKey + "\"");

		// Panel/error strings, read at runtime by FVedgText (same CSVs as the Godot addon).
		RuntimeDependencies.Add("$(PluginDir)/Resources/Translations/*.csv", StagedFileType.UFS);

		// Native crash handler, staged next to the game exe with its branding generated from Project Settings.
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Editor && ProjectDir != null)
		{
			foreach (string FileName in new string[] { "vedg_crash_handler.exe", "crash_banner.jpg", "crash_icon.png" })
			{
				RuntimeDependencies.Add("$(TargetOutputDir)/handler/" + FileName, "$(PluginDir)/Resources/CrashHandler/Win64/" + FileName, StagedFileType.NonUFS);
			}

			string IniFile = Path.Combine(ProjectDir, "Config", "DefaultVedgSDK.ini");
			ExternalDependencies.Add(IniFile);
			string ConfigFile = Path.Combine(PluginDirectory, "Intermediate", "CrashHandler", "crash_handler.json");
			WriteIfChanged(ConfigFile, CrashConfigJson(IniFile));
			RuntimeDependencies.Add("$(TargetOutputDir)/handler/crash_handler.json", ConfigFile, StagedFileType.NonUFS);
		}
	}

	// Same fields as vedg_crash_handler_export_plugin.gd. Empty settings are left out so the handler's own defaults apply.
	static string CrashConfigJson(string IniFile)
	{
		Dictionary<string, string> Settings = ReadIniSection(IniFile, "/Script/VedgSDK.VedgSDKSettings");
		string[][] Fields =
		{
			new[] { "CrashWindowTitle", "window_title" },
			new[] { "CrashProductName", "product_name" },
			new[] { "CrashAccentColor", "accent_color" },
			new[] { "CrashTitle", "crash_title" },
			new[] { "CrashMessage", "crash_message" },
			new[] { "CrashSendLogsLabel", "send_logs_label" },
			new[] { "CrashRestartButton", "restart_button" },
			new[] { "CrashCloseButton", "close_button" },
		};

		StringBuilder Json = new StringBuilder("{\n\t\"banner_image\": \"crash_banner.jpg\",\n\t\"icon_image\": \"crash_icon.png\"");
		foreach (string[] Field in Fields)
		{
			string Value;
			if (Settings.TryGetValue(Field[0], out Value) && Value != "")
			{
				Json.Append(",\n\t\"" + Field[1] + "\": " + JsonString(Value));
			}
		}
		string SendLogs;
		if (Settings.TryGetValue("bCrashSendLogsDefault", out SendLogs))
		{
			Json.Append(",\n\t\"send_logs_default\": " + (SendLogs.ToLowerInvariant() == "true" ? "true" : "false"));
		}
		return Json.Append("\n}\n").ToString();
	}

	static Dictionary<string, string> ReadIniSection(string IniFile, string Section)
	{
		Dictionary<string, string> Values = new Dictionary<string, string>();
		if (!File.Exists(IniFile))
		{
			return Values;
		}
		bool bInSection = false;
		foreach (string RawLine in File.ReadAllLines(IniFile))
		{
			string Line = RawLine.Trim();
			if (Line.StartsWith("["))
			{
				bInSection = Line == "[" + Section + "]";
				continue;
			}
			int Equals = Line.IndexOf('=');
			if (bInSection && Equals > 0)
			{
				Values[Line.Substring(0, Equals).Trim()] = Line.Substring(Equals + 1).Trim();
			}
		}
		return Values;
	}

	// A quoted ini value already uses JSON-compatible escapes (\n, \", \\); a bare one is escaped here.
	static string JsonString(string IniValue)
	{
		if (IniValue.Length >= 2 && IniValue.StartsWith("\"") && IniValue.EndsWith("\""))
		{
			return IniValue;
		}
		return "\"" + IniValue.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"";
	}

	static void WriteIfChanged(string FilePath, string Content)
	{
		if (File.Exists(FilePath) && File.ReadAllText(FilePath) == Content)
		{
			return;
		}
		Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
		File.WriteAllText(FilePath, Content);
	}
}
