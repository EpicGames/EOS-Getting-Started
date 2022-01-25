// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Achievements;
using System.Diagnostics;

namespace EOSCSharpSample.Services
{
    public static class AchievementsService
    {
        public static void QueryDefinitions()
        {
            var queryLeaderboardDefinitionsOptions = new QueryDefinitionsOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
            };

            ViewModelLocator.Main.StatusBarText = $"Querying achievement definitions...";

            App.Settings.PlatformInterface.GetAchievementsInterface().QueryDefinitions(queryLeaderboardDefinitionsOptions, null, (OnQueryDefinitionsCompleteCallbackInfo onQueryDefinitionsCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"QueryDefinitions {onQueryDefinitionsCompleteCallbackInfo.ResultCode}");

                if (onQueryDefinitionsCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    var getAchievementDefinitionCountOptions = new GetAchievementDefinitionCountOptions();
                    var achievementDefinitionCount = App.Settings.PlatformInterface.GetAchievementsInterface().GetAchievementDefinitionCount(getAchievementDefinitionCountOptions);

                    for (uint i = 0; i < achievementDefinitionCount; i++)
                    {
                        var copyAchievementDefinitionByIndexOptions = new CopyAchievementDefinitionV2ByIndexOptions()
                        {
                            AchievementIndex = i
                        };
                        var result = App.Settings.PlatformInterface.GetAchievementsInterface().CopyAchievementDefinitionV2ByIndex(copyAchievementDefinitionByIndexOptions, out var achievementDefinition);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.Achievements.Achievements.Add(achievementDefinition);
                        }
                    }
                }

                AddNotification();

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void QueryPlayerAchievements()
        {
            var queryPlayerAchievementsOptions = new QueryPlayerAchievementsOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
            };

            ViewModelLocator.Main.StatusBarText = $"Querying player achievements...";

            App.Settings.PlatformInterface.GetAchievementsInterface().QueryPlayerAchievements(queryPlayerAchievementsOptions, null, (OnQueryPlayerAchievementsCompleteCallbackInfo onQueryPlayerAchievementsCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"QueryPlayerAchievements {onQueryPlayerAchievementsCompleteCallbackInfo.ResultCode}");

                if (onQueryPlayerAchievementsCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    var getPlayerAchievementCountOptions = new GetPlayerAchievementCountOptions()
                    {
                        UserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
                    };
                    var playerAchievementCount = App.Settings.PlatformInterface.GetAchievementsInterface().GetPlayerAchievementCount(getPlayerAchievementCountOptions);

                    for (uint i = 0; i < playerAchievementCount; i++)
                    {
                        var copyPlayerAchievementByIndexOptions = new CopyPlayerAchievementByIndexOptions()
                        {
                            LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                            TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                            AchievementIndex = i
                        };
                        var result = App.Settings.PlatformInterface.GetAchievementsInterface().CopyPlayerAchievementByIndex(copyPlayerAchievementByIndexOptions, out var playerAchievement);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.Achievements.PlayerAchievements.Add(playerAchievement);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void UnlockAchievement(DefinitionV2 achievementDefinition)
        {
            var unlockAchievementsOptions = new UnlockAchievementsOptions()
            {
                UserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                AchievementIds = new string[] { achievementDefinition.AchievementId }
            };

            ViewModelLocator.Main.StatusBarText = $"Unlocking <{achievementDefinition.AchievementId}>...";

            App.Settings.PlatformInterface.GetAchievementsInterface().UnlockAchievements(unlockAchievementsOptions, null, (OnUnlockAchievementsCompleteCallbackInfo onUnlockAchievementsCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"UnlockAchievements {onUnlockAchievementsCompleteCallbackInfo.ResultCode}");

                if (onUnlockAchievementsCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.Main.StatusBarText = "Successfully unlocked.";
                }
                else
                {
                    Debug.WriteLine("Unlock failed: " + onUnlockAchievementsCompleteCallbackInfo.ResultCode);
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static ulong AddNotification()
        {
            var addNotifyAchievementsUnlockedV2Options = new AddNotifyAchievementsUnlockedV2Options();
            return App.Settings.PlatformInterface.GetAchievementsInterface().AddNotifyAchievementsUnlockedV2(addNotifyAchievementsUnlockedV2Options, null, AchievementsUnlockedCallback);
        }

        private static void AchievementsUnlockedCallback(OnAchievementsUnlockedCallbackV2Info data)
        {
            Debug.WriteLine("Achievement unlocked: " + data.AchievementId);
            // Additional achievement unlock logic goes here
        }

        public static void RemoveNotification(ulong notificationId)
        {
            App.Settings.PlatformInterface.GetAchievementsInterface().RemoveNotifyAchievementsUnlocked(notificationId);
        }
    }
}
