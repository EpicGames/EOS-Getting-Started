# Epic Online Services - Getting Started Guide

[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](LICENSE)

Welcome to the repository for the **Epic Online Services (EOS)** Getting Started series. This repository contains source code accompanying the various tutorials found on the [Epic Online Services blog](https://dev.epicgames.com/news) and the [Epic Developer Community](https://dev.epicgames.com/community/). 

The tutorials focus on getting started with EOS, using C# and Unreal Engine with the Online Subsystem (OSS). The content covers topics such as authentication, player presence, achievements, leaderboards, and more.

## C# Code Samples

The table below links to C#-focused blog posts and the corresponding code in this repository. These blog posts can be found on the Epic Online Services blog, providing guidance on how to use EOS with C#. The full list of blog posts in this series can be found in the [Introduction to Epic Online Services (EOS)](https://dev.epicgames.com/news/introduction-to-epic-online-services-eos#series-reference).

| Blog Post Title | Code Folder | Minimum SDK Version |
|-----------------|-------------|---------------------|
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

For more detailed instructions on using the C# code, refer to the [readme file in the CSharp folder](CSharp).

## Unreal Engine Online Subsystem (OSS) Code

This section contains the source code of an Unreal Engine project built using the **Third Person Feature Pack**. The project demonstrates how to integrate the [Epic Online Services Online Subsystem plugin](https://docs.unrealengine.com/5.1/en-US/online-subsystem-eos-plugin-in-unreal-engine/) into your game. The code corresponds to the course: [The EOS Online Subsystem (OSS) Plugin](https://dev.epicgames.com/community/learning/courses/1px/unreal-engine-online-services-the-eos-online-subsystem-oss-plugin/Lnjn/unreal-engine-online-services-introduction).

The Unreal Engine code is structured around real-world scenarios and is up to **module 7 - EOS P2P, Lobbies and Voice**. Below is a table listing the modules included in the course.

| Module | Release Date |
|--------|--------------|
| Plugin Configuration | Released |
| Signing-in | Released |
| Setting up a dedicated server to host EOS Sessions | Released |
| Joining EOS Session | Released |
| EOS Stats, Achievements, and Leaderboards | Released |
| EOS Player and Title Data Storage | Released |
| EOS P2P, Lobbies, and Voice | Released |
| Voice on Trusted Server | TBD |
| Easy Anti-Cheat | TBD |

### Important Note

If you're working with EOS P2P, make sure to use **Unreal Engine version 5.3**, which includes EOS SDK version 1.16. If you are using an older version of Unreal Engine, you may need to upgrade the EOS SDK. For guidance, refer to [Upgrading the EOS SDK](https://docs.unrealengine.com/5.2/en-US/upgrading-the-eos-sdk-in-unreal-engine/). 

Older versions of the EOS SDK may be affected by a [WebRTC vulnerability](https://eoshelp.epicgames.com/s/news/eos-news-article-MCVDBTZSVM7VAJHF4ZGJVXZM52I4?language=en_US).

## Reporting Issues

If you encounter any issues with the code, please file an [issue](../../issues). We welcome contributions to improve this repository.

## License

The code in this repository is distributed under the terms of the MIT License. See the [LICENSE](LICENSE) file for details.

## Code of Conduct

We are committed to providing a friendly, safe, and welcoming environment for all contributors. Please be respectful, considerate, and inclusive when interacting with others. Harassment and discrimination of any kind will not be tolerated.

By participating in this project, you agree to follow the community guidelines, respect others, and help maintain a welcoming environment for everyone.

If you encounter any issues or have concerns, please report them to the project maintainers.

Resources:

- [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/)
- [Microsoft Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/)
- Contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any questions or concerns.
