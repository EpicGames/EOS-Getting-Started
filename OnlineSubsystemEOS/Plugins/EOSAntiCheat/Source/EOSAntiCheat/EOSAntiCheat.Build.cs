// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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

		// Must mirror EOS_OSS_Tutorial.Build.cs - gates the plugin's peer-mode APIs.
		PrivateDefinitions.Add("P2PMODE=1");
	}
}
