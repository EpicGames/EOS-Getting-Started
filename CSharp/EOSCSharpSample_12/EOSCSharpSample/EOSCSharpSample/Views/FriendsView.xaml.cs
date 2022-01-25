// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class FriendsView : UserControl
    {
        public FriendsViewModel ViewModel { get { return ViewModelLocator.Friends; } }

        public FriendsView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }

        private void FriendsListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ViewModel.PresenceQuery.RaiseCanExecuteChanged();
        }
    }
}
