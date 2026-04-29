# Follow-ups

Tracked items that are out of scope for the current tutorial commits. Grouped by who owns them.

## Engine team (Unreal Engine)

### 1. Expose `FSocketEOS` helpers so external plugins can drive the socket

Source: Tutorial 9 (P2P AC branch).

`FSocketEOS::SetSocketName(const FString&)` and
`FSocketEOS::SetLocalAddress(const FInternetAddrEOS&)` are declared `public` in
`Engine/Plugins/Online/SocketSubsystemEOS/Source/SocketSubsystemEOS/Public/SocketEOS.h`
but are missing the `SOCKETSUBSYSTEMEOS_API` export macro. External plugins
can reach the class via the public header but the linker can't resolve
those two setters, so the intended "create a named FSocketEOS backed by
the EOS P2P transport" flow only works from inside the `SocketSubsystemEOS`
module itself (e.g. `NetDriverEOS.cpp`).

Same goes for the 2-arg `FSocketSubsystemEOS::GetLocalBindAddr(const UWorld* const, FOutputDevice&)`
— also unexported.

**Workaround in this tutorial:** `Plugins/EOSAntiCheat` calls raw
`EOS_P2P_SendPacket` / `EOS_P2P_ReceivePacket` with an explicit channel
hash matching what `FSocketEOS` would use. Works, but duplicates logic
the UE abstraction already implements. The header comment in
`EOSAntiCheatClient.h` (P2PMODE block) points at this issue.

**Fix:** add `SOCKETSUBSYSTEMEOS_API` to the three methods.

### 2. `UIpConnection::SendToRemote` crashes on host shutdown with multi-peer EOS NetDriver

Source: Tutorial 9 (P2P AC branch, 3-client kick test).

`Engine/Plugins/Online/OnlineSubsystemUtils/Source/OnlineSubsystemUtils/Private/IpConnection.cpp:613-614`:

```cpp
ISocketSubsystem* const SocketSubsystem = Driver->GetSocketSubsystem();
SendResult.Error = SocketSubsystem->GetLastErrorCode();   // null deref
```

No null check on `SocketSubsystem`. `UNetDriverEOS::GetSocketSubsystem()` can
return null during `UGameEngine::PreExit -> ShutdownWorldNetDriver -> ...->
UNetDriver::Shutdown -> FlushNet` if the cached `FSocketSubsystemEOS` has
been torn down before the NetDriver — which apparently happens with multiple
live `UNetConnection`s during PreExit.

Reproduces with 3+ peers in a P2P lobby on UE 5.8; harmless on 2-peer
sessions because there's only one connection to flush.

**Fix:** add a null check around line 614, e.g.

```cpp
if (ISocketSubsystem* const SocketSubsystem = Driver->GetSocketSubsystem())
{
    SendResult.Error = SocketSubsystem->GetLastErrorCode();
    HandleSocketSendResult(SendResult, SocketSubsystem);
}
```

### 3. Per-socket reliability on `FSocketEOS`

Source: Tutorial 9 (P2P AC branch).

`FSocketEOS::PacketReliability` is a private member initialized once from
the global `[SocketSubsystemEOS] PacketReliabilityType=` config (see
`SocketEOS.cpp` ctor). Every instance on the process uses the same value;
there's no public setter. This forces a single reliability choice for
`NetDriverEOS`'s game socket AND any custom socket a plugin might want to
create (e.g. AC control messages need `ReliableOrdered`, game replication
typically wants `UnreliableUnordered` and layers its own reliability on
top).

**Fix:** expose a per-socket setter (`FSocketEOS::SetPacketReliability`)
or an extended constructor. Together with follow-up #1, this would let
the EOSAntiCheat plugin use `FSocketEOS` cleanly instead of raw SDK.

## EOS OSS team (Online Subsystem)

### 4. `ReadLeaderboardsForFriends` crashes in 5.8

Source: Tutorial 5 (leaderboards). See the inline comment at
`Source/EOS_OSS_Tutorial/EOSPlayerState.cpp:122-132`.

`IOnlineLeaderboards::ReadLeaderboardsForFriends()` asserts in the UE 5.8
EOS OSS plugin:

- `Engine/Plugins/Online/OnlineSubsystemEOS/.../OnlineLeaderboardsEOS.cpp:327`
  calls `GetFriendsList(LocalUserNum, FString(), Friends)` — empty friends-list
  name.
- `GetFriendsList()` calls `EFriendsLists::FromString(Type, TEXT(""))` in
  `OnlineFriendsInterface.h`.
- That function's fallthrough branch ends with `checkNoEntry()` in 5.8, so
  the call crashes.

Confirmed still present in `++Fortnite+Main` at time of writing.

**Fix:** pass `EFriendsLists::ToString(EFriendsLists::Default)` instead of
`FString()` at the call site in `OnlineLeaderboardsEOS.cpp:327`.

**Workaround in this tutorial:** `AEOSPlayerState::QueryLeaderboardFriends`
does the two-step flow manually — `ReadFriendsList("default")` first,
build the friend-id array, then `ReadLeaderboards(FriendIds, ...)`. Revert
to the one-call `ReadLeaderboardsForFriends()` path once the upstream fix
lands.

## Future tutorial work

### 5. JWT verify in the P2P branch

Source: Tutorial 9 (P2P AC branch).

P2P mesh mode currently trusts the PUIDs reported by the OSS
lobby-participant events for `RegisterPeer` — acceptable because the EAC
SDK's own peer-auth handshake validates client integrity. If a future
feature adds peer-specific lobby attributes that require signed identity
(custom lobby metadata, peer-issued match tokens, etc.), wire the
existing `IEOSAntiCheat::VerifyIdToken` into the peer-register path the
same way the server-mode branch does before `RegisterClient`.

### 6. Consolidate the two local-creds config files

Source: Tutorial 10 (ecom — surfaced when adding the `EcomClient` artifact).

EOS artifact credentials live in two parallel gitignored files that each
serve a different config-load path:

- `Restricted/NoRedist/Config/DefaultEngine.ini` — read by the editor
  / non-packaged runs (UE's standard NoRedist overlay).
- `Config/Windows/WindowsEngine.ini` — read at cook time and baked into
  the Windows pak; the only file the runtime sees in packaged builds.

Adding a new artifact requires editing both, with no automation linking
them. We hit this with `EcomClient`: editor-time worked from NoRedist,
but packaged runtime hit `GetSelectedArtifactSettings ... no settings
found` until the same entry was added to `Config/Windows/WindowsEngine.ini`.

**Options to consider during the bug-fix / cleanup pass:**

1. **Drop NoRedist for credentials.** `Config/Windows/WindowsEngine.ini`
   is a UE-standard platform overlay that's also read at editor time on
   Windows hosts — verify and delete the NoRedist artifact entries. EAC
   certs at `Restricted/NoRedist/EACCerts/` stay (that's a non-config
   asset path passed via `-CertDir`).
2. **Keep NoRedist, drop the Windows overlay.** Add `Restricted/NoRedist/`
   to the cooker's "include in stage" list so its files end up in the pak.
   Riskier — NoRedist is conventionally never-shipped.

(1) is simpler and matches the convention: "platform overlays for
per-platform credentials, NoRedist for actively-don't-redistribute
files." Good cleanup target during the bug-fix pass.

### 7. Add tutorial-section banner headers + renumber tutorials

Source: Tutorial 10 (ecom) introduced a clear visual banner over its
function block in `EOSPlayerController.cpp`:

```cpp
// =====================================================================
// Tutorial 10: Ecom (Store + Purchase + Entitlements) - new flow.
// =====================================================================
```

Apply the same shape to every other tutorial's function group across
the project (login, sessions, voice, AC server-mode, AC P2P branch,
title / player data storage, stats / leaderboards / achievements, etc.).
Makes the .cpps scannable and gives learners a consistent "this is
where Tutorial N lives" landmark.

Tutorial numbers themselves are also being reordered by the user — the
new sequence will land at the same time as this banner pass, so existing
`Tutorial N:` per-line markers need updating in lockstep with the headers.

### 8. Document the four-cell `ACMODE × P2PMODE` matrix in the project README

Source: ACMODE flag commit.

The two compile flags `ACMODE` and `P2PMODE` (both in
`EOS_OSS_Tutorial.Build.cs`) combine orthogonally to form four
build-mode cells:

|             | `ACMODE=0`        | `ACMODE=1`               |
|-------------|-------------------|--------------------------|
| `P2PMODE=0` | server, no AC (committed default) | server + AC |
| `P2PMODE=1` | mesh, no AC       | mesh + AC                |

Currently the topology and AC stories are explained on the
`EOSAntiCheat` plugin README, but the project root README hasn't been
updated since 5.1. After the renumbering pass (#7), the project README
should grow a short "build modes" section pointing at the matrix and
linking to the AC plugin README for the AC-specific cells.

### 8.5. `BuildCookRun -archive` silently skips locked .exe overwrites

Source: P2P+ACMODE test session.

If a packaged game / `start_protected_game.exe` is still running while
you re-run `BuildCookRun ... -archive -archivedirectory=...`, Windows
file-locks the destination `EOS_OSS_Tutorial.exe` inside the archive
tree. UAT logs `BUILD SUCCESSFUL` regardless and the older .exe stays
in place. `ProtectEOSPackage` then hashes the **stale** binary into
the integrity catalog, producing a package that boots fine, passes EAC
integrity, and runs the old code.

This wasted ~30 minutes of testing today (multiple "rebuilds" that
appeared to succeed but kept loading the same pre-fix binary).

Defenses to consider during the bug-fix sweep:

1. **Pre-flight in our `ProtectEOSPackage` UAT** — compare the
   timestamp / size of `Packaged/Windows/EOS_OSS_Tutorial/Binaries/Win64/EOS_OSS_Tutorial.exe`
   against `Saved/StagedBuilds/Windows/EOS_OSS_Tutorial/Binaries/Win64/EOS_OSS_Tutorial.exe`.
   Abort with a clear error if the staged file is newer than the
   archived one - "Package wasn't updated, did you forget to close
   running clients?".
2. **Check tasklist before running** - Powershell or simple `tasklist`
   filter for `EOS_OSS_Tutorial.exe` / `start_protected_game.exe`
   before BuildCookRun. Refuse if any match.
3. **Document loudly** in the plugin README / CLAUDE.md that any
   running protected client must be closed before running
   BuildCookRun.

(1) is the highest-confidence catch since it works on the actual
build artifact rather than process names.

### 8a. Single source of truth for `P2PMODE` across project + plugin Build.cs

Source: P2P+ACMODE test session — hit a confusing crash that turned out
to be a `P2PMODE` mismatch between the project module
(`Source/EOS_OSS_Tutorial/EOS_OSS_Tutorial.Build.cs`) and the plugin
module (`Plugins/EOSAntiCheat/Source/EOSAntiCheat/EOSAntiCheat.Build.cs`).
The plugin's Build.cs has a hardcoded `PrivateDefinitions.Add("P2PMODE=0")`
with a comment "Must mirror EOS_OSS_Tutorial.Build.cs". When a learner
flips the project-side flag and forgets the plugin-side flag, the two
modules end up with different `P2PMODE` values → ABI mismatch when the
project calls into the plugin's peer-mode-conditional code →
`EXCEPTION_ACCESS_VIOLATION` deep inside the AC plugin's multicast
delegate machinery at first BeginPlay. Compiles fine, crashes with no
clear pointer to the root cause.

Two-part fix during the bug-fix sweep:

**(a)** Auto-sync at build time. Plugin's `EOSAntiCheat.Build.cs` reads
the project's `EOS_OSS_Tutorial.Build.cs` and derives `P2PMODE` from
it, so there is one source of truth:

```csharp
string projectBuildCs = Path.Combine(Target.ProjectFile.Directory.FullName,
    "Source", "EOS_OSS_Tutorial", "EOS_OSS_Tutorial.Build.cs");
Match m = Regex.Match(File.ReadAllText(projectBuildCs),
    @"PrivateDefinitions\.Add\(\s*""P2PMODE=(?<v>[01])""\s*\)");
PrivateDefinitions.Add("P2PMODE=" + (m.Success ? m.Groups["v"].Value : "0"));
```

**(b)** Loud warning on the project-side line so the comment is at the
file the learner *edits* (not the file they don't open):

```csharp
// Tutorial 7: P2P mode toggle.
// IMPORTANT: This used to require flipping the matching line in
//   Plugins/EOSAntiCheat/Source/EOSAntiCheat/EOSAntiCheat.Build.cs
// too. As of [bug-fix sweep commit hash], the plugin's Build.cs auto-
// reads this file at build time, so flipping here alone is enough.
// Mismatched values used to cause a confusing crash inside the AC
// plugin's delegate machinery (EXCEPTION_ACCESS_VIOLATION at first
// BeginPlay).
PrivateDefinitions.Add("P2PMODE=0");
```

Apply the same auto-sync pattern to `ACMODE` while we're there - same
risk profile if a future commit puts an `ACMODE` define inside the
plugin's Build.cs.

### 8b. Update `CLAUDE.md` to remove the `bIsUsingP2PSockets` instruction

Source: P2P+ACMODE test session prep.

`CLAUDE.md` currently says:

> The P2P path also requires flipping
> `[/Script/SocketSubsystemEOS.NetDriverEOSBase] bIsUsingP2PSockets=True`
> in `Config/DefaultEngine.ini`. Rebuild after toggling.

That guidance is **stale**. In UE 5.8, `bIsUsingP2PSockets` is
deprecated; the engine actively warns if it's set
(`Engine/Plugins/Online/SocketSubsystemEOS/Source/SocketSubsystemEOS/Private/NetDriverEOS.cpp:21`):

```
"bIsUsingP2PSockets is deprecated, please remove any related config values"
```

The project's `Config/DefaultEngine.ini` already doesn't have it (the
P2P NetDriver is selected purely by the `+NetDriverDefinitions` swap to
`/Script/SocketSubsystemEOS.NetDriverEOS` already in the file). Drop
the `bIsUsingP2PSockets` paragraph from CLAUDE.md during the bug-fix
sweep.

### 9. Audit what else should live on the new `AEOSGameState` class

Source: client-side `RegisterPlayer` sync fix.

`AEOSGameState` was added to mirror `PlayerArray` -> OSS
`RegisteredPlayers` for the server-mode dedicated-server flow. It's a
new GameState surface that runs on every peer (server + every client),
which makes it a natural home for any other "engine event arriving on
this peer that should trickle into OSS state" hooks. Candidates to
review during the bug-fix sweep:

- Score / stats rollup that today might be on-controller and miss
  remote APlayerStates.
- Match-state transitions (`HandleBeginPlay`, `HandleMatchHasStarted`,
  `HandleMatchHasEnded`) — currently driven from the GameMode on the
  server. If any client-side behavior should fire on the same edges, a
  GameState override is the right hook.
- Any future "I noticed peer X joined" event sourced today from
  controller-side replication callbacks.

Don't pre-emptively move things; wait until the bug-fix sweep surfaces
the next concrete need.

### 10. Reconcile `EOSAntiCheat` plugin README with `EOSAntiCheat.Build.cs`

Source: ACMODE flag commit (noticed in passing).

The README's "Step 1: package the game" section claims:

> During `-stage`, `EOSAntiCheat.Build.cs` automatically copies
> `start_protected_game.exe` and the `EasyAntiCheat/` runtime folder
> into the package root via `RuntimeDependencies`. No manual file copy
> required.

This contradicts the Build.cs comment block at line 31:

> No RuntimeDependencies: bootstrapper + EasyAntiCheat/ must live at
> the package ROOT (integrity tool's `-target_game_dir`), not
> Binaries/Win64. UE's path variables don't resolve to the package
> root, so file placement moved to ProtectEOSPackage (post-stage UAT
> command).

The Build.cs is right — the bootstrapper lands via the UAT command's
explicit `File.Copy`, not via `RuntimeDependencies`. Fix the README's
Step 1 description to match.

