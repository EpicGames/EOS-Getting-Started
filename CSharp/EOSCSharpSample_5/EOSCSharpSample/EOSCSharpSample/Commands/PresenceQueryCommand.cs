// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;

namespace EOSCSharpSample.Commands
{
    public class PresenceQueryCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.AccountId);
        }

        public override void Execute(object parameter)
        {
            if (parameter == null)
            {
                PresenceService.Copy(EpicAccountId.FromString(ViewModelLocator.Main.AccountId), EpicAccountId.FromString(ViewModelLocator.Main.AccountId));
            }
        }
    }
}
