// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using EOSCSharpSample.Models;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class FriendsViewModel : BindableBase
    {
        private ObservableCollection<Friend> _friends;
        public ObservableCollection<Friend> Friends
        {
            get { return _friends; }
            set { SetProperty(ref _friends, value); }
        }

        private Friend _selectedFriend;
        public Friend SelectedFriend
        {
            get { return _selectedFriend; }
            set { SetProperty(ref _selectedFriend, value); }
        }

        private ulong _notificationId;
        public ulong NotificationId
        {
            get { return _notificationId; }
            set { SetProperty(ref _notificationId, value); }
        }

        public FriendsQueryCommand FriendsQuery { get; set; }
        public FriendsSubscribeUpdatesCommand FriendsSubscribeUpdates { get; set; }
        public FriendsUnsubscribeUpdatesCommand FriendsUnsubscribeUpdates { get; set; }
        public PresenceQueryCommand PresenceQuery { get; set; }

        public FriendsViewModel()
        {
            FriendsQuery = new FriendsQueryCommand();
            FriendsSubscribeUpdates = new FriendsSubscribeUpdatesCommand();
            FriendsUnsubscribeUpdates = new FriendsUnsubscribeUpdatesCommand();
            PresenceQuery = new PresenceQueryCommand();
        }
    }
}
