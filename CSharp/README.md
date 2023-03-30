# Epic Online Services - Getting Started C# content

[![License](https://img.shields.io/github/license/mashape/apistatus.svg)](../LICENSE)

This repository contains the source code that accompanies the Epic Online Services C# tutorials, found 
on the [Epic Online Services blog](https://dev.epicgames.com/news). 

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

## Requirements

The solutions in this repository require Visual Studio 2019 or Visual Studio 2022 with the following workloads & features:

* C#
  * Desktop & Mobile > .NET desktop development

## Usage

To compile and run each solution, follow these steps:

1. Download the latest C# Epic Online Services SDK on the [Epic Online Services website](https://dev.epicgames.com/sdk).
2. Copy the `SDK\Source` directory from the downloaded SDK ZIP to the `EOSCSharpSample\SDK` folder.
3. Copy the `EOSSDK-Win32-Shipping.dll` file from the 'SDK\Bin' directory of the downloaded SDK ZIP to the `EOSCSharpSample\EOSCSharpSample` folder.
4. Set up a product in the [Epic Games Developer Portal](https://dev.epicgames.com/portal/) (sign-up required) and add your product's SDK credentials to the `EOSCSharpSample\EOSCSharpSample\ApplicationSettings.cs` file.