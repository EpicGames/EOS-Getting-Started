// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Leaderboards;
using System.Diagnostics;

namespace EOSCSharpSample.Services
{
    public static class LeaderboardsService
    {
        public static void QueryDefinitions()
        {
            var queryLeaderboardDefinitionsOptions = new QueryLeaderboardDefinitionsOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
            };

            ViewModelLocator.Main.StatusBarText = $"Querying leaderboard definitions...";

            App.Settings.PlatformInterface.GetLeaderboardsInterface().QueryLeaderboardDefinitions(ref queryLeaderboardDefinitionsOptions, null, (ref OnQueryLeaderboardDefinitionsCompleteCallbackInfo onQueryLeaderboardDefinitionsCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"QueryLeaderboardDefinitions {onQueryLeaderboardDefinitionsCompleteCallbackInfo.ResultCode}");

                if (onQueryLeaderboardDefinitionsCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    var getLeaderboardDefinitionCountOptions = new GetLeaderboardDefinitionCountOptions();
                    var leaderboardDefinitionCount = App.Settings.PlatformInterface.GetLeaderboardsInterface().GetLeaderboardDefinitionCount(ref getLeaderboardDefinitionCountOptions);

                    for (uint i = 0; i < leaderboardDefinitionCount; i++)
                    {
                        var copyLeaderboardDefinitionByIndexOptions = new CopyLeaderboardDefinitionByIndexOptions()
                        {
                            LeaderboardIndex = i
                        };
                        var result = App.Settings.PlatformInterface.GetLeaderboardsInterface().CopyLeaderboardDefinitionByIndex(ref copyLeaderboardDefinitionByIndexOptions, out var leaderboardDefinition);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.Leaderboards.Leaderboards.Add(leaderboardDefinition.Value);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void QueryRanks(Definition leaderboard)
        {
            var queryLeaderboardRanksOptions = new QueryLeaderboardRanksOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                LeaderboardId = leaderboard.LeaderboardId
            };

            ViewModelLocator.Main.StatusBarText = $"Querying leaderboard ranks...";

            App.Settings.PlatformInterface.GetLeaderboardsInterface().QueryLeaderboardRanks(ref queryLeaderboardRanksOptions, null, (ref OnQueryLeaderboardRanksCompleteCallbackInfo onQueryLeaderboardRanksCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"QueryLeaderboardRanks {onQueryLeaderboardRanksCompleteCallbackInfo.ResultCode}");

                if (onQueryLeaderboardRanksCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    var getLeaderboardRecordCountOptions = new GetLeaderboardRecordCountOptions();
                    var leaderboardRecordCount = App.Settings.PlatformInterface.GetLeaderboardsInterface().GetLeaderboardRecordCount(ref getLeaderboardRecordCountOptions);

                    for (uint i = 0; i < leaderboardRecordCount; i++)
                    {
                        var copyLeaderboardRecordByIndexOptions = new CopyLeaderboardRecordByIndexOptions()
                        {
                            LeaderboardRecordIndex = i
                        };
                        var result = App.Settings.PlatformInterface.GetLeaderboardsInterface().CopyLeaderboardRecordByIndex(ref copyLeaderboardRecordByIndexOptions, out var leaderboardRecord);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.Leaderboards.LeaderboardRecords.Add(leaderboardRecord.Value);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }
    }
}
