// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;

namespace EOSCSharpSample.Commands
{
    public class TitleStorageReadFileCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return ViewModelLocator.TitleStorage.SelectedTitleStorageFile != null;
        }

        public override void Execute(object parameter)
        {
            TitleStorageService.ReadFile(ViewModelLocator.TitleStorage.SelectedTitleStorageFile);
        }
    }
}
