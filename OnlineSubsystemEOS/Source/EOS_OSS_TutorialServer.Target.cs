// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// Tutorial 10: Dedicated-server target. Built and packaged separately from the
// Game target so the anti-cheat server interface compiles in (WITH_EDITOR=0)
// and BeginSession / RegisterClient / kick callbacks actually run. The editor
// Server.local.bat launch (UnrealEditor.exe -server) still works for day-to-day
// iteration without anti-cheat; for end-to-end AC tests package this target
// and run UnrealGameServer.exe directly.
public class EOS_OSS_TutorialServerTarget : TargetRules
{
	public EOS_OSS_TutorialServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("EOS_OSS_Tutorial");
	}
}
