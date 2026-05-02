// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EOS_OSS_Tutorial : ModuleRules
{
	public EOS_OSS_Tutorial(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemEOS" });

        // Tutorial 11 (voice chat) module deps:
        //   - VoiceChat + EOSVoiceChat: the IVoiceChat abstraction and its EOS RTC implementation.
        //   - EOSShared: EOS platform-handle access and EOS SDK string helpers.
        //   - HTTP + Json: REST + JSON for the dedicated server's Voice Web API calls.
        PrivateDependencyModuleNames.AddRange(new string[] { "VoiceChat", "EOSVoiceChat", "EOSShared", "HTTP", "Json", "EOSAntiCheat" });

        // Tutorial 4: Network topology toggle. Single source of truth - the EOSAntiCheat
        // plugin's Build.cs reads this file at build time and mirrors the value, so flipping
        // here is enough.
        PrivateDefinitions.Add("P2PMODE=0");

        // Tutorial 10: Toggle the EOS Anti-Cheat plugin's tutorial-side wiring on/off.
        //   ACMODE=0 (default) - skip every AC call site; the plugin module still
        //                        loads but is dormant. Tutorials 1-9 and 11 work
        //                        without configuring the EAC bootstrapper / integrity tool.
        //                        Plain BuildCookRun output is the shippable artifact;
        //                        do NOT run RunUAT.bat ProtectEOSPackage on this build.
        //   ACMODE=1           - run the AC flow exactly as Tutorial 10 describes.
        //                        Required: bootstrapper + integrity tool + EAC certs.
        //                        Run RunUAT.bat ProtectEOSPackage after BuildCookRun.
        // Combines orthogonally with P2PMODE - all four (ACMODE, P2PMODE) cells
        // must compile and run.
        PrivateDefinitions.Add("ACMODE=0");
    }
}
