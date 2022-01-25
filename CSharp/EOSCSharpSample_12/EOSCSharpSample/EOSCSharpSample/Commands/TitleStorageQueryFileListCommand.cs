// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.TitleStorage;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class TitleStorageQueryFileListCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId) && !string.IsNullOrWhiteSpace(ViewModelLocator.TitleStorage.TitleStorageTag);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.TitleStorage.TitleStorageFiles = new ObservableCollection<FileMetadata>();
            TitleStorageService.QueryFileList(ViewModelLocator.TitleStorage.TitleStorageTag);
        }
    }
}
