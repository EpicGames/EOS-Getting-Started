// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.Leaderboards;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class LeaderboardsViewModel : BindableBase
    {
        private ObservableCollection<Definition> _leaderboards;
        public ObservableCollection<Definition> Leaderboards
        {
            get { return _leaderboards; }
            set { SetProperty(ref _leaderboards, value); }
        }

        private ObservableCollection<LeaderboardRecord> _leaderboardRecords;
        public ObservableCollection<LeaderboardRecord> LeaderboardRecords
        {
            get { return _leaderboardRecords; }
            set { SetProperty(ref _leaderboardRecords, value); }
        }

        private Definition _selectedLeaderboard;
        public Definition SelectedLeaderboard
        {
            get { return _selectedLeaderboard; }
            set { SetProperty(ref _selectedLeaderboard, value); }
        }

        public LeaderboardsQueryDefinitionsCommand LeaderboardsQueryDefinitions { get; set; }
        public LeaderboardsQueryRanksCommand LeaderboardsQueryRanks { get; set; }

        public LeaderboardsViewModel()
        {
            LeaderboardsQueryDefinitions = new LeaderboardsQueryDefinitionsCommand();
            LeaderboardsQueryRanks = new LeaderboardsQueryRanksCommand();
        }
    }
}
