# Epic Online Services - Getting Started content

[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](LICENSE)

This repository contains the source code that accompanies the various Epic Online Services tutorials, found 
on the [Epic Online Services blog](https://dev.epicgames.com/news) and [Epic Developer Community](https://dev.epicgames.com/community/). 
The content covers getting started using C# and Unreal Engine using the Online Subsystem (OSS). 

## C# code

Refer to the table below to get to the code that accompanies each C#-focused blog post found on the Epic Online Services blog. The full list of blog posts in the series can be 
found in the series reference of the [Introduction to Epic Online Services (EOS)](https://dev.epicgames.com/news/introduction-to-epic-online-services-eos#series-reference) post.

| Blog post | Folder | Min. SDK version |
| --- | --- | --- |
| [3. Setting up a C# solution for EOS in Visual Studio 2019](https://dev.epicgames.com/news/setting-up-a-c-solution-for-eos-in-visual-studio-2019) | [CSharp/EOSCSharpSample_3](CSharp/EOSCSharpSample_3) | 1.15.2 |
| [4. Authenticating with Epic Account Services (EAS)](https://dev.epicgames.com/news/player-authentication-with-epic-account-services-eas) | [CSharp/EOSCSharpSample_4](CSharp/EOSCSharpSample_4) | 1.15.2 |
| [5. Getting and setting user presence](https://dev.epicgames.com/en-US/news/getting-and-setting-player-presence) | [CSharp/EOSCSharpSample_5](CSharp/EOSCSharpSample_5) | 1.15.2 |
| [6. Querying for Epic Friends and their status](https://dev.epicgames.com/news/querying-for-epic-friends-and-their-status) | [CSharp/EOSCSharpSample_6](CSharp/EOSCSharpSample_6) | 1.15.2 |
| [7. Accessing EOS Game Services with the Connect Interface](https://dev.epicgames.com/news/accessing-eos-game-services-with-the-connect-interface) | [CSharp/EOSCSharpSample_7](CSharp/EOSCSharpSample_7) | 1.15.2 |
| [8. Retrieve game-specific data from the cloud](https://dev.epicgames.com/news/retrieve-game-specific-data-from-the-cloud) | [CSharp/EOSCSharpSample_8](CSharp/EOSCSharpSample_8) | 1.15.2 |
| [9. Storing and retrieving player-specific data](https://dev.epicgames.com/news/storing-and-retrieving-player-specific-data) | [CSharp/EOSCSharpSample_9](CSharp/EOSCSharpSample_9) | 1.15.2 |
| [10. Manage player statistics with Epic Online Services](https://dev.epicgames.com/news/manage-player-statistics-with-epic-online-services) | [CSharp/EOSCSharpSample_10](CSharp/EOSCSharpSample_10) | 1.15.2 |
| [11. Rank player scores using leaderboards](https://dev.epicgames.com/news/rank-player-scores-using-leaderboards) | [CSharp/EOSCSharpSample_11](CSharp/EOSCSharpSample_11) | 1.15.2 |
| [12. Adding achievements to your game](https://dev.epicgames.com/news/adding-achievements-to-your-game) | [CSharp/EOSCSharpSample_12](CSharp/EOSCSharpSample_12) | 1.15.2 |

Learn more about the requirements and how to use the C# code in the [readme file in the CSharp folder](CSharp).

## Unreal Engine Online Subsystem (OSS) code

This is the code of an Unreal Engine project built using the Third Person feature pack. The project shows how to integrate our [Epic Online Services Online Subsystem plugin.](https://docs.unrealengine.com/5.1/en-US/online-subsystem-eos-plugin-in-unreal-engine/) in a game. This code accompanies this course: [The EOS Online Subsystem (OSS) Plugin](https://dev.epicgames.com/community/learning/courses/1px/unreal-engine-online-services-the-eos-online-subsystem-oss-plugin/Lnjn/unreal-engine-online-services-introduction)  The table below lists the module entries in this course. Unlike the Csharp code, the UE code isn't structured per tutorial, but instead structured in a way it could be done in a real game. **The code currently is up to module 7 - EOS P2P, Lobbies and Voice** 

| Module | Release Date |
| --- | --- |
| Plugin Configuration | Released |
| Signing-in | Released|
| Setting up a dedicated server to host EOS Sessions | Released |
| Joining EOS Session | Released |
| EOS Stats, Achievements and Leaderboards | Released |
| EOS Player and Title Data Storage | Released |
| EOS P2P, Lobbies and Voice | Released |
| Voice on Trusted Server | Early 2024 |
| Easy Anti-Cheat | Early 2024 |

### Important Note
If you are using EOS P2P, you should use Unreal Engine version 5.3 as it is bundled with EOS SDK 1.16. Or, you should upgrade the EOS SDK version in older version of UE, see [Upgrading the EOS SDK](https://docs.unrealengine.com/5.2/en-US/upgrading-the-eos-sdk-in-unreal-engine/). Older versions of the EOS SDK have this [WebRTC vulnerability](https://eoshelp.epicgames.com/s/news/eos-news-article-MCVDBTZSVM7VAJHF4ZGJVXZM52I4?language=en_US). 

## Issues

If you notice a mistake in the code, please file an [issue](../../issues).

## License

Getting Started series code is distributed under the terms of MIT License. See the [LICENSE](LICENSE) for details.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).

Resources:

- [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/)
- [Microsoft Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/)
- Contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with questions or concerns
