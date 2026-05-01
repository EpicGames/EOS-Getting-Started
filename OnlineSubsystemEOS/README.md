# Epic Online Services - Getting Started Unreal Engine Online Subsystem content

[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](../LICENSE)

This repository contains the source code that accompanies [The EOS Online Subsystem (OSS) Plugin course](https://dev.epicgames.com/community/learning/courses/1px/unreal-engine-online-services-the-eos-online-subsystem-oss-plugin/Lnjn/unreal-engine-online-services-introduction) that is a getting started guide on using [The EOS Online Subsystem (OSS) Plugin](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine).

## Requirements

The solution in this repository is a C++ Unreal Engine project targeting version 5.8. To build the project you will need a source or binary build of Unreal Engine. You will also need to have set up Visual Studio for working with Unreal Engine C++ projects. See [Setting Up Visual Studio](https://docs.unrealengine.com/5.8/en-US/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine/). The EOS SDK shipped with UE 5.8 is current; older UE versions had a [WebRTC vulnerability](https://eoshelp.epicgames.com/s/news/eos-news-article-MCVDBTZSVM7VAJHF4ZGJVXZM52I4?language=en_US) you should avoid.

## Usage

To compile and run the project, follow these steps:

1. If you're using a source build of Unreal Engine, build your engine.
2. In the root folder, right-click on the .uproject file and select "Generate Visual Studio Project Files".
3. Open the solution in Visual Studio, and build it.

**The course is a walkthrough on how this code was written.**

## Build modes

Two compile flags in `Source/EOS_OSS_Tutorial/EOS_OSS_Tutorial.Build.cs` combine orthogonally to form a four-cell build matrix:

- **`P2PMODE`** picks the network topology.
  - `P2PMODE=0` *(default)*: Dedicated server. Server creates an EOS Session, clients find/join via OSS, replication runs over `NetDriverEOS` against the dedicated server.
  - `P2PMODE=1`: Listen-server over EOS P2P transport. One lobby member calls `GetWorld()->Listen(...)` and becomes the UE listen server (`NM_ListenServer`); other lobby members `ClientTravel` to the host's PUID via `NetDriverEOS`. EOS Lobby is used (`bUseLobbiesIfAvailable=true`) instead of an EOS Session, with `bUseLobbiesVoiceChatIfAvailable=true` providing built-in voice. Note: this is **not** true mesh replication — it's listen-server topology where the transport happens to be EOS P2P.
- **`ACMODE`** toggles the EOS Anti-Cheat plugin's call sites on/off.
  - `ACMODE=0` *(default)*: AC plugin module compiles but every game-side call site is skipped. Tutorials 1-9 and 11 work without configuring the EAC bootstrapper / integrity tool.
  - `ACMODE=1`: Full AC flow per Tutorial 10. In `P2PMODE=0` the dedicated server runs as EAC Server; in `P2PMODE=1` every participant runs EAC Client in **`PeerToPeer` mode** (design choice — the listen-server host as EAC Server is a valid alternative the tutorial doesn't take).

The four cells:

|             | `ACMODE=0`                                | `ACMODE=1`                                            |
|-------------|-------------------------------------------|-------------------------------------------------------|
| `P2PMODE=0` | Dedicated server, no AC *(default)*       | Dedicated server + EAC Server                         |
| `P2PMODE=1` | Listen-server over EOS P2P, no AC         | Listen-server over EOS P2P + EAC Client `PeerToPeer`  |

See [`Plugins/EOSAntiCheat/README.md`](Plugins/EOSAntiCheat/README.md) for AC-specific setup (bootstrapper, integrity tool, EAC certs).

To switch modes, edit the two `PrivateDefinitions.Add(...)` lines in `EOS_OSS_Tutorial.Build.cs` (and the matching `P2PMODE=...` line in `Plugins/EOSAntiCheat/Source/EOSAntiCheat/EOSAntiCheat.Build.cs`), then rebuild.
