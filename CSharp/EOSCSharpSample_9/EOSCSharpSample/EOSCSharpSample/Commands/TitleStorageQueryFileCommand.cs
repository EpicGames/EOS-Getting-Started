// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.TitleStorage;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class TitleStorageQueryFileCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId) && !string.IsNullOrWhiteSpace(ViewModelLocator.TitleStorage.TitleStorageFileName);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.TitleStorage.TitleStorageFiles = new ObservableCollection<FileMetadata>();
            TitleStorageService.QueryFile(ViewModelLocator.TitleStorage.TitleStorageFileName);
        }
    }
}
