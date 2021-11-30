// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;

namespace EOSCSharpSample.Commands
{
    public class FriendsSubscribeUpdatesCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.AccountId) && (ViewModelLocator.Friends.Friends.Count != 0) && (ViewModelLocator.Friends.NotificationId == 0);
        }

        public override void Execute(object parameter)
        {
            FriendsService.SubscribeUpdates();
        }
    }
}
