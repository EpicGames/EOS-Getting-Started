# EOS Anti-Cheat plugin

Project-level plugin that wraps the `EOS_AntiCheat{Server,Client}` SDK
interfaces and integrates with this tutorial's existing session + voice
flows. The runtime code compiles out of Editor targets
(`#if !WITH_EDITOR`) - Anti-Cheat only runs under the EasyAntiCheat
bootstrapper, which wraps packaged Game / Client builds. In-editor PIE
isn't a protected process and will stay that way.

> **Threading.** The EOS SDK is not thread-safe. Every plugin entry
> point and every plugin delegate runs on the game thread (the same
> thread `OnlineSubsystemEOS` ticks `EOS_Platform_Tick` on). Don't move
> plugin work onto a worker thread or stand up a second platform tick —
> see the threading note in `IEOSAntiCheat.h` for the full contract.

## Supported topologies

This plugin supports two topologies, matching the tutorial project's
`P2PMODE` compile flag:

- `P2PMODE=0` — **Dedicated server.** EAC Server + Client interfaces.
  AC messages ride normal Unreal `Server_/Client_` RPCs; the dedicated
  server connect URL is `127.0.0.1:7777` (or a real server IP), so
  `NetDriverEOS` passes through to `IpNetDriver` and the actual transport
  is plain IP. Server flags via `OnClientActionRequired` and kicks via
  `AGameSession::KickPlayer`.
- `P2PMODE=1` — **P2P mesh.** EAC Client only, in `PeerToPeer` mode.
  Every participant registers every other participant as a peer via the
  OSS lobby-participant events; the plugin owns a dedicated EOS P2P
  socket (`EOSAntiCheat`) for AC message transport, separate from the
  game's `NetDriverEOS` socket. Each peer enforces locally on
  `OnPeerActionRequired(RemovePlayer)` — the SDK's *"full mesh
  peer-to-peer mode"* has no central authority.

The peer-mode code in this plugin is `#if P2PMODE`-guarded and only
compiles when the tutorial module is built with `P2PMODE=1` (the flag
is duplicated in `EOSAntiCheat.Build.cs` — keep it in sync with
`EOS_OSS_Tutorial.Build.cs`).

Listen-server is not a supported deployment target.

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
UE toolchain. The pipeline is identical for server-mode and P2P-mesh
builds; only the `-Artifact` argument in Step 2 differs.

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

**Server-mode build (`P2PMODE=0`) — uses the `Client` artifact by default:**

```bat
<Engine>\Engine\Build\BatchFiles\RunUAT.bat ProtectEOSPackage ^
    -ScriptsForProject=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -Project=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -PackageDir=<FullPathTo>\Packaged\WindowsClient
```

**P2P-mesh build (`P2PMODE=1`) — pass `-Artifact=P2PClient`:**

```bat
<Engine>\Engine\Build\BatchFiles\RunUAT.bat ProtectEOSPackage ^
    -ScriptsForProject=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -Project=<FullPathTo>\EOS_OSS_Tutorial.uproject ^
    -PackageDir=<FullPathTo>\Packaged\WindowsClient ^
    -Artifact=P2PClient
```

`-ScriptsForProject` tells UAT to pick up the project's automation
scripts (including this one); `-Project` is our command's own
parameter telling it where to read artifact IDs from.

The command:

1. Reads the chosen artifact (`Client` or `P2PClient`) from the
   project's `Config/DefaultEngine.ini`. The committed file ships with
   blank artifact stubs — fill in your `ProductId` / `SandboxId` /
   `DeploymentId` / `ClientId` / `ClientSecret` from the EOS Dev Portal
   before running this step (same credentials Tutorial 1 walks through).
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
| `-Artifact=<name>` | `Client` | Which DefaultEngine.ini artifact to read IDs from. Use `P2PClient` for P2P-mesh builds. |
| `-IntegrityLabel=<label>` | `base_public` | Dev Portal label for the uploaded manifest |

Re-run Step 2 whenever you repackage (the manifest is tied to the
hash of the exact staged binaries). Step 1 isn't required if the only
thing you changed is the Settings or cfg.

### Step 3: launch

`.bat` templates for both modes live in `batch/` at the project root;
see `batch/README.md`. Quick reference:

- Server-mode client: `batch/Client.Protected.bat` (through the
  bootstrapper) or `batch/Client.Direct.bat` (unprotected, expected kick).
- Server-mode dedicated server: `batch/Server.Packaged.bat` (servers
  never launch through the bootstrapper - EAC server API is
  trusted-process-only).
- P2P-mesh client: `batch/Client.P2P.Protected.bat` (through the
  bootstrapper, `-epicapp="P2PClient"`) or `batch/Client.P2P.Direct.bat`
  (unprotected, expected kick by protected peers).

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
