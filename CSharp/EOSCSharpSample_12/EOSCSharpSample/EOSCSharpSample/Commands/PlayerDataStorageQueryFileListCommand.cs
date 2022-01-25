// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.PlayerDataStorage;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class PlayerDataStorageQueryFileListCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.PlayerDataStorage.PlayerDataStorageFiles = new ObservableCollection<FileMetadata>();
            PlayerDataStorageService.QueryFileList();
        }
    }
}
