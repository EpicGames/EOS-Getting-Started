# Launch scripts

`.bat` templates for running the tutorial — editor-launch scripts used
by Tutorials 2-7, and the packaged-build / Anti-Cheat launch scripts
added in Tutorial 9. Originally scattered across the project root; all
now live here.

## Layout

- `batch/*.bat` — committed templates with placeholders (`<Path to ...>`,
  `<DevAuthTool Credential name>`). Safe to ship publicly.
- `batch/local/*.bat` — your real paths + DevAuthTool credentials. The
  entire `local/` folder is gitignored (see `.gitignore`), so filenames
  don't need a `.local` suffix — the directory name marks them as local.

**Workflow:** copy `batch/Foo.bat` → `batch/local/Foo.bat`, edit paths /
credentials, run the local copy.

## Files in this folder

### Editor-launch (Tutorials 2-7, both modes)

| File | Purpose |
|---|---|
| `Client.bat` | Launch the game in-editor as a standalone client. |
| `Client2.bat` | Same, for a second client window. |
| `Server.bat` | Launch the game in-editor as a dedicated server (`-server -log`). |

These use `UnrealEditor.exe <uproject> ...` and target the in-editor
build, so Anti-Cheat is compiled out (`#if !WITH_EDITOR`). For the full
Tutorial 9 flow use the packaged-build scripts below.

### Server mode (`P2PMODE=0`)

| File | Purpose |
|---|---|
| `Server.Packaged.bat` | Run the packaged dedicated server (no bootstrapper — EAC server API is trusted-process-only). |
| `Client.Protected.bat` | Run a packaged client **through** `start_protected_game.exe`. Expected: EAC handshake completes, stays connected. |
| `Client2.Protected.bat` | Same, for a second client with a different DevAuthTool credential. |
| `Client.Direct.bat` | Run the packaged client **without** the bootstrapper. Expected: server kicks this client after the auth timeout. |
| `Client2.Direct.bat` | Same, for the second client. |

### P2P mesh mode (`P2PMODE=1`)

| File | Purpose |
|---|---|
| `Client.P2P.Protected.bat` | P2P peer launched through the bootstrapper. Uses `-epicapp="P2PClient"` so the OSS picks the `P2PClient` artifact credentials. |
| `Client2.P2P.Protected.bat` | Same, second peer with a different DevAuthTool credential. |
| `Client3.P2P.Protected.bat` | Third protected peer — used with one `Direct` to exercise the non-host's `Server_PeerViolationDetected` telemetry RPC (host + 1 protected non-host + 1 unprotected non-host). |
| `Client.P2P.Direct.bat` | Unprotected P2P peer — expected to be kicked by protected peers via `OnPeerActionRequired` after the configured timeout. |
| `Client2.P2P.Direct.bat` | Same, second unprotected peer. |
| `Client3.P2P.Direct.bat` | Same, third unprotected peer. |

No dedicated server exists in P2P mesh mode (by project design — this
tutorial supports dedicated server OR P2P mesh, not listen-server).

## Packaging pipeline (both modes)

See `Plugins/EOSAntiCheat/README.md` for the full flow. In short:

1. **Package**: `RunUAT.bat BuildCookRun -clientconfig=Development -cook
   -allmaps -build -stage -pak -archive` (add `-server -serverconfig=...`
   for the server-mode dedicated server).
2. **Protect**: `RunUAT.bat ProtectEOSPackage -PackageDir=<staged>`. For
   P2P, also pass `-Artifact=P2PClient` so `Settings.json` and the
   integrity manifest carry the `P2PClient` Dev Portal credentials.
3. **Launch**: run a `.local.bat` from this folder.
