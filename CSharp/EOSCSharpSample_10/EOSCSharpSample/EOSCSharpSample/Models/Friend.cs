// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using Epic.OnlineServices;
using Epic.OnlineServices.Friends;
using Epic.OnlineServices.Presence;

namespace EOSCSharpSample.Models
{
    public class Friend : BindableBase
    {
        private EpicAccountId _epicAccountId;
        public EpicAccountId EpicAccountId
        {
            get { return _epicAccountId; }
            set { SetProperty(ref _epicAccountId, value); }
        }

        private FriendsStatus _friendsStatus;
        public FriendsStatus FriendsStatus
        {
            get { return _friendsStatus; }
            set { SetProperty(ref _friendsStatus, value); }
        }

        private Status _status;
        public Status Status
        {
            get { return _status; }
            set { SetProperty(ref _status, value); }
        }
    }
}
