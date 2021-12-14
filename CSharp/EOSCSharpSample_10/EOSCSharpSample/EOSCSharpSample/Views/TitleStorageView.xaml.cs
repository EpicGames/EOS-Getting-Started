// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class TitleStorageView : UserControl
    {
        public TitleStorageViewModel ViewModel { get { return ViewModelLocator.TitleStorage; } }

        public TitleStorageView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }

        private void TitleStorageFilesListView_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ViewModel.TitleStorageReadFile.RaiseCanExecuteChanged();
        }

        private void TitleStorageFileNameTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            ViewModel.TitleStorageQueryFile.RaiseCanExecuteChanged();
        }

        private void TitleStorageTagTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            ViewModel.TitleStorageQueryFileList.RaiseCanExecuteChanged();
        }
    }

}
