# EOS Anti-Cheat plugin

Project-level plugin that wraps the `EOS_AntiCheat{Server,Client}` SDK
interfaces and integrates with this tutorial's existing session + voice
flows. The runtime code compiles out of Editor targets
(`#if !WITH_EDITOR`) - Anti-Cheat only runs under the EasyAntiCheat
bootstrapper, which wraps packaged Game / Client builds. In-editor PIE
isn't a protected process and will stay that way.

## One-time setup

1. **Dev Portal.** Create an Anti-Cheat Service configuration under your
   EOS Product and link it to the Deployment used by your `Client` and
   `Server` artifacts. Enable the Anti-Cheat permission set on both
   client policies.

2. **Extract the EAC tools.** The engine ships an `EOS_AntiCheatTools-*.zip`
   under `Engine/Source/ThirdParty/EOSSDK/SDK/Tools/`. Extract its
   contents into this plugin's `Tools/` directory:

   ```
   Plugins/EOSAntiCheat/Tools/
       devtools/
           anticheat_integritytool.exe
           anticheat_integritytool.cfg
       dist/
           start_protected_game.exe
           EasyAntiCheat/
               EasyAntiCheat_EOS_Setup.exe
               Settings.json      ← template; overwritten per-package by ProtectPackage.ps1
               ...
   ```

   `Tools/` is gitignored - EAC binaries are not redistributable, so
   every developer extracts their own copy.

## Packaging flow

Two UAT commands. Both are RunUAT-native - no shell scripting outside the
UE toolchain.

### Step 1: package the game

Either Editor menu (`File > Package Project > Windows Client`) or command
line:

```bat
<Engine>\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun ^
    -project=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -noP4 -platform=Win64 -clientconfig=Development ^
    -cook -allmaps -build -stage -pak -archive ^
    -archivedirectory=<FullPathTo>\Packaged
```

During `-stage`, `EOSAntiCheat.Build.cs` automatically copies
`start_protected_game.exe` and the `EasyAntiCheat/` runtime folder into
the package root via `RuntimeDependencies`. No manual file copy
required.

### Step 2: protect the package

Run the plugin's custom UAT command:

```bat
<Engine>\Engine\Build\BatchFiles\RunUAT.bat ProtectEOSPackage ^
    -ScriptsForProject=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -Project=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -PackageDir=<FullPathTo>\Packaged\WindowsClient
```

`-ScriptsForProject` tells UAT to pick up the project's automation
scripts (including this one); `-Project` is our command's own
parameter telling it where to read artifact IDs from.

The command:

1. Reads the `Client` artifact from the project's DefaultEngine.ini,
   with the per-developer `Restricted/NoRedist/Config/DefaultEngine.ini`
   overlay taking precedence (so the committed stub with blank
   credentials doesn't win).
2. Writes `<PackageDir>/EasyAntiCheat/Settings.json` with the resolved
   ProductId / SandboxId / DeploymentId and the game exe name.
3. Invokes `Plugins/EOSAntiCheat/Tools/devtools/anticheat_integritytool.exe`
   with those credentials and the tool's cfg, which hashes the packaged
   tree and uploads the manifest to the Dev Portal under the label
   `base_public`.

Command-line overrides:

| Flag | Default | Purpose |
|---|---|---|
| `-GameExe=<file.exe>` | `<uproject stem>.exe` | Name of the packaged game binary at the package root |
| `-Artifact=<name>` | `Client` | Which DefaultEngine.ini artifact to read IDs from |
| `-IntegrityLabel=<label>` | `base_public` | Dev Portal label for the uploaded manifest |

Re-run Step 2 whenever you repackage (the manifest is tied to the
hash of the exact staged binaries). Step 1 isn't required if the only
thing you changed is the Settings or cfg.

### Step 3: launch

```bat
<FullPathTo>\Packaged\WindowsClient\start_protected_game.exe
```

`start_protected_game.exe` reads `EasyAntiCheat/Settings.json`,
installs the EAC runtime (one-time UAC prompt on first launch on this
machine), then executes the game binary named in `Settings.json`.
`IEOSAntiCheatClient` now reports itself as available and the plugin's
`BeginSession` call succeeds.

Server builds launch normally (no bootstrapper) - AntiCheat servers are
not player-facing processes and the SDK doesn't require protection
there.

## Expected logs (packaged Win64 client under the bootstrapper)

```
LogEOSAntiCheatPlugin: [FEOSAntiCheat::StartupModule] EOS AntiCheat plugin initialized.
LogEOSAntiCheatPlugin: Verbose: [FEOSAntiCheatClient::BeginSession] Session started (mode=ClientServer, localUser=<PUID>).
```

Server:

```
LogEOSAntiCheatPlugin: Verbose: [FEOSAntiCheatServer::BeginSession] Session started.
LogEOSAntiCheatPlugin: Verbose: [FEOSAntiCheatServer::RegisterClient] Registered <PUID>.
```

If the client is NOT launched through the bootstrapper, the EOS SDK
prints:

```
LogEOSSDK: LogEOSAntiCheat: [AntiCheatClient] Anti-cheat client not available.
    Verify that the game was started using the anti-cheat bootstrapper
    if you intend to use it.
```

This is the SDK's own log category (`LogEOSAntiCheat`, not
`LogEOSAntiCheatPlugin`) confirming it saw no bootstrapper.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `ProtectPackage.ps1` 401 / 403 on upload | `Client` policy is missing the Anti-Cheat upload permission. Fix in the Dev Portal. |
| `start_protected_game.exe` exits with "integrity failed" | Package changed since the last integrity run. Re-run `ProtectPackage.ps1`. |
| `Anti-cheat client not available` in a packaged launch | `EasyAntiCheat/Settings.json` is missing or has wrong `productid`/`deploymentid`. Re-run `ProtectPackage.ps1`. |
| Kicks without apparent cause in packaged build | Dev Portal Anti-Cheat policy is restrictive by default. Check the server log for `[FEOSAntiCheatServer::FlagForKick]` - the reason string comes straight from the SDK. |
