// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Text.RegularExpressions;
using UnrealBuildTool;

public class EOSAntiCheat : ModuleRules
{
	public EOSAntiCheat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"CoreOnline",  // FUniqueNetIdRef in the public delegate signature
			"Engine",
		});

		// OnlineSubsystemEOS gives us the EOS_HPlatform; EOSShared the helper types;
		// EOSSDK the C headers for EOS_AntiCheat{Server,Client}.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemEOS",
			"EOSShared",
			"EOSSDK",
		});

		// No RuntimeDependencies: bootstrapper + EasyAntiCheat/ must live at the
		// package ROOT (integrity tool's -target_game_dir), not Binaries/Win64.
		// UE's path variables don't resolve to the package root, so file placement
		// moved to ProtectEOSPackage (post-stage UAT command).

		// Single source of truth for P2PMODE: read the project's
		// EOS_OSS_Tutorial.Build.cs at build time and mirror its value here. The flag
		// gates the plugin's peer-mode APIs; if the two modules disagreed it produced
		// an EXCEPTION_ACCESS_VIOLATION deep inside the AC plugin's delegate
		// machinery at first BeginPlay (compiles fine, crashes with no clear pointer).
		// Falls back to "0" if the project file or define line is unreadable - matches
		// the project's committed default.
		string ProjectBuildCsPath = (Target.ProjectFile != null)
			? Path.Combine(Target.ProjectFile.Directory.FullName,
				"Source", "EOS_OSS_Tutorial", "EOS_OSS_Tutorial.Build.cs")
			: null;
		string P2PModeValue = "0";
		if (ProjectBuildCsPath != null && File.Exists(ProjectBuildCsPath))
		{
			Match Match = Regex.Match(
				File.ReadAllText(ProjectBuildCsPath),
				@"PrivateDefinitions\.Add\(\s*""P2PMODE=(?<v>[01])""\s*\)");
			if (Match.Success)
			{
				P2PModeValue = Match.Groups["v"].Value;
			}
		}
		PrivateDefinitions.Add("P2PMODE=" + P2PModeValue);
	}
}
