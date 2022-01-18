// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Stats;
using System.Diagnostics;

namespace EOSCSharpSample.Services
{
    public static class StatsService
    {
        public static void Ingest(int count)
        {
            var ingestStatOptions = new IngestStatOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Stats = new IngestData[]
                {
                    new IngestData() { StatName = "SumStat", IngestAmount = count },
                    new IngestData() { StatName = "LatestStat", IngestAmount = count },
                    new IngestData() { StatName = "MinStat", IngestAmount = count },
                    new IngestData() { StatName = "MaxStat", IngestAmount = count }
                }
            };

            ViewModelLocator.Main.StatusBarText = $"Ingesting stats (count: <{count}>)...";

            App.Settings.PlatformInterface.GetStatsInterface().IngestStat(ingestStatOptions, null, (IngestStatCompleteCallbackInfo ingestStatCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"IngestStat {ingestStatCompleteCallbackInfo.ResultCode}");

                if (ingestStatCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.Stats.Clicks = 0;
                }
                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void Query()
        {
            var queryStatsOptions = new QueryStatsOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
            };

            ViewModelLocator.Main.StatusBarText = $"Querying stats...";

            App.Settings.PlatformInterface.GetStatsInterface().QueryStats(queryStatsOptions, null, (OnQueryStatsCompleteCallbackInfo onQueryStatsCompleteCallbackInfo) =>
            {
                Debug.WriteLine($"QueryStats {onQueryStatsCompleteCallbackInfo.ResultCode}");

                if (onQueryStatsCompleteCallbackInfo.ResultCode == Result.Success)
                {
                    var getStatCountOptions = new GetStatCountOptions()
                    {
                        TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
                    };
                    var statCount = App.Settings.PlatformInterface.GetStatsInterface().GetStatsCount(getStatCountOptions);

                    for (uint i = 0; i < statCount; i++)
                    {
                        var copyStatByIndexOptions = new CopyStatByIndexOptions()
                        {
                            StatIndex = i,
                            TargetUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
                        };
                        var result = App.Settings.PlatformInterface.GetStatsInterface().CopyStatByIndex(copyStatByIndexOptions, out var stat);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.Stats.Stats.Add(stat);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }
    }
}
