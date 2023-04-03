// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EOS_OSS_Tutorial : ModuleRules
{
	public EOS_OSS_Tutorial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemEOS" });
	}
}
