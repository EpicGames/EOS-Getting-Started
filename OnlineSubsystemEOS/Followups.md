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

