// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using AutomationTool;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace EOSAntiCheat
{
	[Help(@"
Finalizes a packaged UE build with EOS Anti-Cheat protection.

Run after RunUAT BuildCookRun has produced a staged Win64 package. The plugin's
Build.cs already staged the EAC runtime files into the package via
RuntimeDependencies; this command:

  1. Writes EasyAntiCheat/Settings.json with deployment IDs pulled from the
     project's [/Script/OnlineSubsystemEOS.EOSSettings] artifact config in
     Config/DefaultEngine.ini.
  2. Runs Plugins/EOSAntiCheat/Tools/devtools/anticheat_integritytool.exe
     against the packaged tree to upload a hash manifest to the Dev Portal.

Invoke:

  RunUAT.bat ProtectEOSPackage -Project=<uproject> -PackageDir=<stagedDir>

After this completes, launch the game via <stagedDir>/start_protected_game.exe.
")]
	[Help("Project", "Path to the .uproject file (required).")]
	[Help("PackageDir", "Root of the staged package produced by BuildCookRun (required).")]
	[Help("GameExe", "Filename of the packaged game binary. Defaults to '<uproject basename>.exe'.")]
	[Help("Artifact", "Artifact to read from EOSSettings. Defaults to 'Client'.")]
	[Help("CertDir", "Directory containing the Dev Portal cert + private key (default: Plugins/EOSAntiCheat/Tools/Certs).")]
	[Help("OutName", "Base filename the integrity tool writes into EasyAntiCheat/Certificates (default: 'base').")]
	public class ProtectEOSPackage : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string ProjectPath = ParseRequiredStringParam("Project");
			string PackageDir = ParseRequiredStringParam("PackageDir");
			string ProjectName = Path.GetFileNameWithoutExtension(ProjectPath);
			string GameExe = ParseParamValue("GameExe") ?? ProjectName + ".exe";
			string Artifact = ParseParamValue("Artifact") ?? "Client";
			string OutName = ParseParamValue("OutName") ?? "base";
			string CertDirOverride = ParseParamValue("CertDir");

			// -------- Resolve paths ----------------------------------------
			string ProjectDir = Path.GetDirectoryName(ProjectPath);
			string PluginDir = Path.Combine(ProjectDir, "Plugins", "EOSAntiCheat");
			string ToolsDir = Path.Combine(PluginDir, "Tools");
			string IntegrityTool = Path.Combine(ToolsDir, "devtools", "anticheat_integritytool.exe");
			string EacDistDir = Path.Combine(ToolsDir, "dist");
			string BootstrapperSource = Path.Combine(EacDistDir, "start_protected_game.exe");
			string EacRuntimeSource = Path.Combine(EacDistDir, "EasyAntiCheat");
			// Use the plugin's committed UE-aware cfg, not the one extracted from the
			// SDK zip. The committed version excludes UE cooked assets (.pak, .uasset,
			// .uexp, ...) and non-executable files (.json, .conf, ...) so the catalog
			// only contains .exe/.dll code and EAC client-side verification stays fast.
			string IntegrityCfg = Path.Combine(PluginDir, "AntiCheatIntegrity.cfg");

			// Refuse if EOS_OSS_Tutorial.Build.cs doesn't declare ACMODE=1:
			// protecting an ACMODE=0 package produces a "protected" build
			// whose game binary never initializes EAC at runtime. Fail-safe -
			// any read/parse failure is treated as ACMODE=0.
			string TutorialBuildCs = Path.Combine(
				ProjectDir, "Source", "EOS_OSS_Tutorial", "EOS_OSS_Tutorial.Build.cs");
			bool bAcModeOne = false;
			if (File.Exists(TutorialBuildCs))
			{
				string BuildCsText = File.ReadAllText(TutorialBuildCs);
				Match AcMatch = Regex.Match(
					BuildCsText,
					@"PrivateDefinitions\.Add\(\s*""ACMODE=(?<v>[01])""\s*\)");
				bAcModeOne = AcMatch.Success && AcMatch.Groups["v"].Value == "1";
			}
			if (!bAcModeOne)
			{
				throw new AutomationException(
					"ACMODE=1 not declared in {0}. ACMODE=0 builds ship un-protected by design "
				  + "- skip this step. To protect, set ACMODE=1 in EOS_OSS_Tutorial.Build.cs and rebuild first.",
					TutorialBuildCs);
			}

			// -------- Preflight --------------------------------------------
			if (!Directory.Exists(PackageDir))
			{
				throw new AutomationException("PackageDir not found: {0}. Run BuildCookRun first.", PackageDir);
			}
			if (!File.Exists(IntegrityTool) || !File.Exists(BootstrapperSource) || !Directory.Exists(EacRuntimeSource))
			{
				throw new AutomationException(
					"EAC tools not fully extracted. Make sure Plugins/EOSAntiCheat/Tools/ contains both 'devtools/anticheat_integritytool.exe' and 'dist/start_protected_game.exe' (+ dist/EasyAntiCheat/). Extract EOS_AntiCheatTools-win32-x64-*.zip into the plugin's Tools/ directory.");
			}

			// Locate the real game binary inside the package. UE Win64 Client packages put
			// a thin launcher at <PackageDir>/<ProjectName>.exe and the real game binary at
			// <PackageDir>/<ProjectName>/Binaries/Win64/<ProjectName>.exe.
			string InnerGameExeRel = Path.Combine(ProjectName, "Binaries", "Win64", GameExe);
			string InnerGameExeAbs = Path.Combine(PackageDir, InnerGameExeRel);
			if (!File.Exists(InnerGameExeAbs))
			{
				throw new AutomationException("Inner game binary not found at {0}. Confirm -PackageDir / -GameExe or repackage.", InnerGameExeAbs);
			}

			// EAC runtime + bootstrapper land at the PACKAGE ROOT so that the integrity
			// tool's -target_game_dir can walk the entire staged tree (Engine/ and project
			// content both) in one pass. Catalog scope == "game root", as EAC expects.
			string BootstrapperDest = Path.Combine(PackageDir, "start_protected_game.exe");
			string EacRuntimeDest = Path.Combine(PackageDir, "EasyAntiCheat");
			string SettingsPath = Path.Combine(EacRuntimeDest, "Settings.json");

			// Integrity tool needs the Dev Portal-issued private key + certificate.
			// Download them from Dev Portal -> your game title -> Anti-Cheat -> Certificates
			// and save under Plugins/EOSAntiCheat/Tools/Certs/ (the entire Tools/ tree is
			// gitignored), or pass -CertDir=<dir>.
			string CertDir = !string.IsNullOrEmpty(CertDirOverride)
				? CertDirOverride
				: Path.Combine(PluginDir, "Tools", "Certs");
			string PrivateKey = Path.Combine(CertDir, "base_private.key");
			string Certificate = Path.Combine(CertDir, "base_public.cer");
			if (!File.Exists(PrivateKey) || !File.Exists(Certificate))
			{
				throw new AutomationException(
					"Missing EAC certificate or private key. Download 'base_private.key' and 'base_public.cer' from your Dev Portal game title's Anti-Cheat Certificates page and save them to {0} (or pass -CertDir=<dir>).",
					CertDir);
			}

			// -------- Read artifact IDs ------------------------------------
			var ArtifactInfo = ReadArtifactFromIni(ProjectDir, Artifact);

			Logger.LogInformation("Protecting package: {0}", PackageDir);
			Logger.LogInformation("  Artifact      = {0}", Artifact);
			Logger.LogInformation("  ProductId     = {0}", ArtifactInfo.ProductId);
			Logger.LogInformation("  SandboxId     = {0}", ArtifactInfo.SandboxId);
			Logger.LogInformation("  DeploymentId  = {0}", ArtifactInfo.DeploymentId);

			// -------- Copy EAC runtime to package root ---------------------
			// start_protected_game.exe has to live at the package root so its
			// cwd-anchored "game root" matches the integrity tool's -target_game_dir
			// (this same PackageDir). Without this alignment, EAC's scope misses
			// DLLs like Engine/Binaries/ThirdParty/Ogg/... and blocks their load.
			File.Copy(BootstrapperSource, BootstrapperDest, overwrite: true);
			CopyDirectoryRecursively(EacRuntimeSource, EacRuntimeDest, skipSettingsJson: true);
			Logger.LogInformation("Copied bootstrapper + EasyAntiCheat runtime to {0}", PackageDir);

			// -------- Write Settings.json ----------------------------------
			// executable path is relative to start_protected_game.exe, which is now
			// at the package root. Point it at the inner game binary that EAC should
			// actually monitor (not the outer thin launcher - EAC needs to hook the
			// real process).
			string InnerGameExeForJson = InnerGameExeRel.Replace('\\', '/');
			string Settings = string.Format(
				"{{\n" +
				"    \"title\"                       : \"{0}\",\n" +
				"    \"executable\"                  : \"{1}\",\n" +
				"    \"productid\"                   : \"{2}\",\n" +
				"    \"sandboxid\"                   : \"{3}\",\n" +
				"    \"deploymentid\"                : \"{4}\",\n" +
				"    \"requested_splash\"            : \"EasyAntiCheat/SplashScreen.png\",\n" +
				"    \"wait_for_game_process_exit\"  : \"false\",\n" +
				"    \"hide_bootstrapper\"           : \"false\",\n" +
				"    \"hide_gui\"                    : \"false\"\n" +
				"}}\n",
				ProjectName, InnerGameExeForJson,
				ArtifactInfo.ProductId, ArtifactInfo.SandboxId, ArtifactInfo.DeploymentId);

			File.WriteAllText(SettingsPath, Settings, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
			Logger.LogInformation("Wrote {0}", SettingsPath);

			// -------- Run integrity tool -----------------------------------
			// The tool generates local signed catalogue files (base.bin / base.conf) under
			// <PackageDir>/EasyAntiCheat/Certificates which the EAC runtime verifies at
			// launch. Catalog scope is <PackageDir> / everything under it, minus the
			// cfg's exclude list (assets, configs, Settings.json, etc.).
			string IntegrityArgs = string.Format(
				"-productid \"{0}\" -inkey \"{1}\" -incert \"{2}\" -outname \"{3}\" -target_game_dir \"{4}\" \"{5}\"",
				ArtifactInfo.ProductId, PrivateKey, Certificate, OutName, PackageDir, IntegrityCfg);

			Logger.LogInformation("Running anticheat_integritytool...");
			IProcessResult Result = Run(IntegrityTool, IntegrityArgs, WorkingDir: PackageDir);
			if (Result.ExitCode != 0)
			{
				throw new AutomationException("anticheat_integritytool failed (exit code {0}). See output above.", Result.ExitCode);
			}

			Logger.LogInformation("Package is protected. Launch via {0}\\start_protected_game.exe", PackageDir);
		}

		/// <summary>
		/// Copy every file under SrcDir into DestDir, preserving relative paths.
		/// If skipSettingsJson is true, the Settings.json template from the SDK zip
		/// is skipped (we write a freshly-configured one in the caller).
		/// </summary>
		private static void CopyDirectoryRecursively(string SrcDir, string DestDir, bool skipSettingsJson)
		{
			Directory.CreateDirectory(DestDir);
			foreach (string Src in Directory.GetFiles(SrcDir, "*", SearchOption.AllDirectories))
			{
				if (skipSettingsJson && Path.GetFileName(Src).Equals("Settings.json", StringComparison.OrdinalIgnoreCase))
				{
					continue;
				}
				string Rel = Src.Substring(SrcDir.Length).TrimStart('\\', '/');
				string Dest = Path.Combine(DestDir, Rel);
				Directory.CreateDirectory(Path.GetDirectoryName(Dest));
				File.Copy(Src, Dest, overwrite: true);
			}
		}


		private struct ArtifactSettings
		{
			public string ProductId;
			public string SandboxId;
			public string DeploymentId;
			public string ClientId;
			public string ClientSecret;
		}

		private ArtifactSettings ReadArtifactFromIni(string ProjectDir, string ArtifactName)
		{
			string IniPath = Path.Combine(ProjectDir, "Config", "DefaultEngine.ini");
			if (!File.Exists(IniPath))
			{
				throw new AutomationException("Config/DefaultEngine.ini not found at {0}.", IniPath);
			}

			// Read the committed config, then transparently layer the per-developer
			// Restricted/NoRedist overlay on top if it exists. This is a UE-standard
			// gitignored layout for local-only credentials; tutorial readers won't
			// have one, in which case only the committed file is consulted.
			var Combined = new StringBuilder();
			Combined.AppendLine(File.ReadAllText(IniPath));
			string OverlayIni = Path.Combine(ProjectDir, "Restricted", "NoRedist", "Config", "DefaultEngine.ini");
			if (File.Exists(OverlayIni))
			{
				Combined.AppendLine(File.ReadAllText(OverlayIni));
			}

			// Match the last +Artifacts=(ArtifactName="<ArtifactName>",...) line so a
			// later override wins (overlay or repeated entries within the same file).
			var Rx = new Regex(
				@"^\+Artifacts=\(ArtifactName=""" + Regex.Escape(ArtifactName) + @""",(?<body>.*)\)\s*$",
				RegexOptions.Multiline);

			Match Last = null;
			foreach (Match M in Rx.Matches(Combined.ToString()))
			{
				Last = M;
			}
			if (Last == null)
			{
				throw new AutomationException("Artifact '{0}' not found in Config/DefaultEngine.ini. Add a +Artifacts= line under [/Script/OnlineSubsystemEOS.EOSSettings] with your Dev Portal credentials.", ArtifactName);
			}

			string Body = Last.Groups["body"].Value;
			ArtifactSettings Result = default;
			Result.ProductId    = ReadField(Body, "ProductId");
			Result.SandboxId    = ReadField(Body, "SandboxId");
			Result.DeploymentId = ReadField(Body, "DeploymentId");
			Result.ClientId     = ReadField(Body, "ClientId");
			Result.ClientSecret = ReadField(Body, "ClientSecret");

			if (string.IsNullOrEmpty(Result.ProductId) || string.IsNullOrEmpty(Result.ClientSecret))
			{
				throw new AutomationException("Artifact '{0}' is missing credentials. Populate ProductId / SandboxId / DeploymentId / ClientId / ClientSecret in Config/DefaultEngine.ini under [/Script/OnlineSubsystemEOS.EOSSettings].", ArtifactName);
			}
			return Result;
		}

		private static string ReadField(string Body, string Field)
		{
			Match M = Regex.Match(Body, Field + @"=""(?<value>[^""]*)""");
			return M.Success ? M.Groups["value"].Value : string.Empty;
		}
	}
}
