// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Models;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class FriendsQueryCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.AccountId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Friends.Friends = new ObservableCollection<Friend>();
            FriendsService.QueryFriends();
        }
    }
}
