// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class PlayerDataStorageView : UserControl
    {
        public PlayerDataStorageViewModel ViewModel { get { return ViewModelLocator.PlayerDataStorage; } }

        public PlayerDataStorageView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }

        private void PlayerDataStorageFilesListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ViewModel.PlayerDataStorageReadFile.RaiseCanExecuteChanged();
            ViewModel.PlayerDataStorageDuplicateFile.RaiseCanExecuteChanged();
            ViewModel.PlayerDataStorageDeleteFile.RaiseCanExecuteChanged();
        }
    }

}
