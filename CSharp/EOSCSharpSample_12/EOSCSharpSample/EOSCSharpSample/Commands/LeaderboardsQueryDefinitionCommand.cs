// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.Leaderboards;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class LeaderboardsQueryDefinitionsCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Leaderboards.Leaderboards = new ObservableCollection<Definition>();
            ViewModelLocator.Leaderboards.LeaderboardRecords = new ObservableCollection<LeaderboardRecord>();
            LeaderboardsService.QueryDefinitions();
        }
    }
}
