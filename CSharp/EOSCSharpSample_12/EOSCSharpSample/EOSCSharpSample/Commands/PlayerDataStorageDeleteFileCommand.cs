// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;

namespace EOSCSharpSample.Commands
{
    public class PlayerDataStorageDeleteFileCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return ViewModelLocator.PlayerDataStorage.SelectedPlayerDataStorageFile != null;
        }

        public override void Execute(object parameter)
        {
            PlayerDataStorageService.DeleteFile(ViewModelLocator.PlayerDataStorage.SelectedPlayerDataStorageFile);
        }
    }
}
