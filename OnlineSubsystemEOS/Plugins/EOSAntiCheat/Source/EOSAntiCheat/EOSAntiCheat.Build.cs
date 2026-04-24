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

		// OnlineSubsystemEOS gives us the already-initialized EOS_HPlatform via
		// IOnlineSubsystemEOS::GetEOSPlatformHandle(); EOSShared exposes the helper
		// types (IEOSPlatformHandlePtr etc.); EOSSDK is the C headers for the
		// EOS_AntiCheat{Server,Client} interfaces we wrap.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"OnlineSubsystemEOS",
			"EOSShared",
			"EOSSDK",
		});

		// Intentionally no RuntimeDependencies for EAC runtime files. The
		// bootstrapper + EasyAntiCheat/ directory must live at the package
		// ROOT (where the integrity tool's -target_game_dir points), not
		// inside $(ProjectDir)/Binaries/Win64. UE's RuntimeDependencies path
		// variables don't resolve to the package root, so file placement
		// moved to the post-stage UAT command (ProtectEOSPackage) - one
		// pass does copy + Settings.json + integrity tool together.
	}
}
