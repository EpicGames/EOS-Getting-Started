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
        PrivateDependencyModuleNames.AddRange(new string[] { "VoiceChat", "EOSVoiceChat", "EOSShared", "HTTP", "Json", "EOSAntiCheat" });

        // Tutorial 7: This will set the game to be in P2P mode instead of dedicated server.
        PrivateDefinitions.Add("P2PMODE=0");

        // Tutorial 9: Toggle the EOS Anti-Cheat plugin's tutorial-side wiring on/off.
        //   ACMODE=0 (default) - skip every AC call site; the plugin module still
        //                        loads but is dormant. Tutorials 4-8 work without
        //                        configuring the EAC bootstrapper / integrity tool.
        //                        Plain BuildCookRun output is the shippable artifact;
        //                        do NOT run RunUAT.bat ProtectEOSPackage on this build.
        //   ACMODE=1           - run the AC flow exactly as Tutorial 9 describes.
        //                        Required: bootstrapper + integrity tool + EAC certs.
        //                        Run RunUAT.bat ProtectEOSPackage after BuildCookRun.
        // Combines orthogonally with P2PMODE - all four (ACMODE, P2PMODE) cells
        // must compile and run.
        PrivateDefinitions.Add("ACMODE=0");
    }
}
