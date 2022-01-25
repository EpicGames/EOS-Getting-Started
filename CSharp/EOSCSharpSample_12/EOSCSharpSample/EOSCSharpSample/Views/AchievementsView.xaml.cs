// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class AchievementsView : UserControl
    {
        public AchievementsViewModel ViewModel { get { return ViewModelLocator.Achievements; } }

        public AchievementsView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }

        private void AchievementsListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ViewModel.AchievementsUnlockAchievement.RaiseCanExecuteChanged();
        }
    }
}
