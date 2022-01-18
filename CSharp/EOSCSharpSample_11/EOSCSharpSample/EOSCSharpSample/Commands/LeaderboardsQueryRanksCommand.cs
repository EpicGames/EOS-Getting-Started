// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.Leaderboards;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class LeaderboardsQueryRanksCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return ViewModelLocator.Leaderboards.SelectedLeaderboard != null;
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Leaderboards.LeaderboardRecords = new ObservableCollection<LeaderboardRecord>();
            LeaderboardsService.QueryRanks(ViewModelLocator.Leaderboards.SelectedLeaderboard);
        }
    }
}
