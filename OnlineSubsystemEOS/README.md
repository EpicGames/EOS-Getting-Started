# Epic Online Services - Unreal Engine OSS Tutorial

[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](../LICENSE)

> ⚠️ **This repo is significantly ahead of the linked course.**
> The [EOS Online Subsystem (OSS) Plugin course](https://dev.epicgames.com/community/learning/courses/1px/unreal-engine-the-eos-online-subsystem-oss-plugin/Lnjn/unreal-engine-introduction)
> still walks through the original module-7 snapshot. Since then this
> project has added several new tutorials, reorganized the numbering,
> upgraded to Unreal Engine 5.8, and picked up bug fixes. To check out
> the snapshot that still matches the course step-by-step, use commit
> `https://github.com/EpicGames/EOS-Getting-Started/commit/e5e2304a07a7f9a879c12be113922cf49f623b91`. A course refresh covering everything past that
> snapshot is planned for **Q3-Q4 2026**.

The project demonstrates how to integrate the [EOS OSS plugin](https://docs.unrealengine.com/5.1/en-US/online-subsystem-eos-plugin-in-unreal-engine/)
into an Unreal Engine 5.8 third-person sample.

## Tutorial coverage

| # | Topic | Primary source |
|---|-------|----------------|
| 0 | Plugin configuration | this README + `Config/DefaultEngine.ini` |
| 1 | Login (EAS + EOS Connect) | `EOSPlayerController` |
| 2 | Stats, achievements, leaderboards | `EOSPlayerState` |
| 3 | Ecom: store offers, purchases, entitlements | `EOSPlayerController` |
| 4 | EOS Lobbies (P2P) + lobby voice | `EOSPlayerController` |
| 5 | EOS Sessions (dedicated server) | `EOSGameSession` + `EOSPlayerController` |
| 6 | Friends, Presence, EOS Social Overlay | `EOSPlayerController` |
| 7 | Title Storage + Player Data Storage | `EOSPlayerController` |
| 8 | Player Reports | `EOSPlayerController` |
| 9 | Sanctions (query + appeal) | `EOSPlayerController` |
| 10 | Anti-Cheat (server-mode + P2P) | [`Plugins/EOSAntiCheat`](Plugins/EOSAntiCheat/README.md) |
| 11 | Server-minted RTC voice tokens | `EOSGameSession` + `EOSPlayerController` |

## Pick what you need

The tutorials are independently usable; you only need the modules that match your shipping plan.

**Tutorials 0 (plugin configuration) and 1 (login) are required for every scenario below** — they're the foundation that every other module builds on.

**Already shipping on Steam / other storefronts and adding EGS:**
- **Single-player:** Tutorials 0-2 (config + login + achievements/stats).
- **+ in-app purchases:** add Tutorial 3.
- **+ cross-store multiplayer (matchmaking + parties):** add Tutorial 4 (Lobbies, for P2P) or Tutorial 5 (Sessions, for dedicated-server). Either acts as the cross-store handshake — clients on different storefronts join the same lobby or session.

**Building a fully-online multiplayer game without backend infrastructure:**
- Use `P2PMODE=1` (Tutorial 4 Lobbies + Tutorial 6 Friends/Presence + Tutorial 10 P2P-branch Anti-Cheat + lobby voice from Tutorial 4). EOS Lobbies + EOS P2P transport + lobby voice + Easy Anti-Cheat in `PeerToPeer` mode is a complete shipping stack — no dedicated servers, no backend boxes, no token-issuance service. The host runs as a UE listen-server over EOS P2P; every peer registers every other peer with EAC.

**Building a dedicated-server game:**
- `P2PMODE=0` (default). Tutorial 5 (Sessions) + Tutorial 10 (server-mode Anti-Cheat) + Tutorial 11 (server-minted voice tokens) cover the full server-mode path.

## Requirements

UE 5.8 (source or binary). Visual Studio set up for UE C++ — see [Setting Up Visual Studio](https://docs.unrealengine.com/5.8/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/). The EOS SDK shipped with UE 5.8 is current; older UE versions had a [WebRTC vulnerability](https://eoshelp.epicgames.com/s/news/eos-news-article-MCVDBTZSVM7VAJHF4ZGJVXZM52I4?language=en_US) you should avoid.

## Usage

1. Source-build users: build the engine first.
2. Right-click the `.uproject` → "Generate Visual Studio Project Files".
3. Open the solution in Visual Studio and build.
4. Fill in your EOS artifact credentials in `Config/DefaultEngine.ini` (Tutorial 0).

## Build modes

Two compile flags in `Source/EOS_OSS_Tutorial/EOS_OSS_Tutorial.Build.cs` combine orthogonally:

- **`P2PMODE`** — `0` for dedicated-server topology (default), `1` for listen-server over EOS P2P transport (one lobby member calls `Listen()`, others `ClientTravel` to the host's PUID via `NetDriverEOS`).
- **`ACMODE`** — `0` (default) skips every EAC call site; the plugin compiles but is dormant, and Tutorials 1-9 and 11 work without EAC setup. `1` runs the full anti-cheat flow per Tutorial 10.

|             | `ACMODE=0` *(default)*                          | `ACMODE=1`                                                  |
|-------------|-------------------------------------------------|-------------------------------------------------------------|
| `P2PMODE=0` | Dedicated server, no AC                         | Dedicated server + EAC Server                               |
| `P2PMODE=1` | Listen-server over EOS P2P, no AC               | Listen-server over EOS P2P + EAC Client `PeerToPeer`        |

In the `P2PMODE=1 / ACMODE=1` cell, EAC `PeerToPeer` mode is a design choice — running the host as EAC Server is also valid but out of scope here. See [`Plugins/EOSAntiCheat/README.md`](Plugins/EOSAntiCheat/README.md) for AC-specific setup. To switch modes, edit the `PrivateDefinitions.Add(...)` lines in `EOS_OSS_Tutorial.Build.cs` and rebuild — the EOSAntiCheat plugin reads `P2PMODE` from the project's Build.cs automatically.
