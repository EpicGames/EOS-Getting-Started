// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EOS_OSS_Tutorial : ModuleRules
{
	public EOS_OSS_Tutorial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemEOS" });

        // Tutorial 8 (voice chat) module deps:
        //   - VoiceChat + EOSVoiceChat: the IVoiceChat abstraction and its EOS RTC implementation.
        //   - EOSShared: EOS platform-handle access and EOS SDK string helpers.
        //   - HTTP + Json: REST + JSON for the dedicated server's Voice Web API calls.
        PrivateDependencyModuleNames.AddRange(new string[] { "VoiceChat", "EOSVoiceChat", "EOSShared", "HTTP", "Json" });

        // Tutorial 7: This will set the game to be in P2P mode instead of dedicated server.
        PrivateDefinitions.Add("P2PMODE=0");
    }
}
