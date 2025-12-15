// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EOS_Example_Android : ModuleRules
{
	public EOS_Example_Android(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemEOS", "Json"});
	}
}
