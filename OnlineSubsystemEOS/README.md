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

Two compile flags in `Source/EOS_OSS_Tutorial/EOS_OSS_Tutorial.Build.cs` combine orthogonally:

- **`P2PMODE`** — `0` for dedicated-server topology (default), `1` for listen-server over EOS P2P transport (not true mesh replication; one lobby member calls `Listen()`, others `ClientTravel` to the host's PUID via `NetDriverEOS`).
- **`ACMODE`** — `0` (default) skips every EAC call site (plugin compiles but is dormant; Tutorials 1-9 and 11 work without EAC setup). `1` runs the full anti-cheat flow per Tutorial 10.

|             | `ACMODE=0` *(default)*                          | `ACMODE=1`                                                  |
|-------------|-------------------------------------------------|-------------------------------------------------------------|
| `P2PMODE=0` | Dedicated server, no AC                         | Dedicated server + EAC Server                               |
| `P2PMODE=1` | Listen-server over EOS P2P, no AC               | Listen-server over EOS P2P + EAC Client `PeerToPeer`        |

In the `P2PMODE=1 / ACMODE=1` cell, EAC `PeerToPeer` mode is a design choice — the listen-server host as EAC Server is also a valid alternative this tutorial doesn't exercise. See [`Plugins/EOSAntiCheat/README.md`](Plugins/EOSAntiCheat/README.md) for AC-specific setup. To switch modes, edit the `PrivateDefinitions.Add(...)` lines in `EOS_OSS_Tutorial.Build.cs` and rebuild — the EOSAntiCheat plugin reads `P2PMODE` from the project's Build.cs automatically.
