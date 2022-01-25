// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class LeaderboardsView : UserControl
    {
        public LeaderboardsViewModel ViewModel { get { return ViewModelLocator.Leaderboards; } }

        public LeaderboardsView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }

        private void LeaderboardsListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ViewModel.LeaderboardsQueryRanks.RaiseCanExecuteChanged();
        }
    }
}
