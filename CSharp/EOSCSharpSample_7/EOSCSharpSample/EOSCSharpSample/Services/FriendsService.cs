// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Models;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Friends;
using System.Diagnostics;
using System.Linq;

namespace EOSCSharpSample.Services
{
    public static class FriendsService
    {
        public static void QueryFriends()
        {
            var queryFriendsOptions = new QueryFriendsOptions()
            {
                LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId)
            };

            ViewModelLocator.Main.StatusBarText = $"Querying friends...";

            App.Settings.PlatformInterface.GetFriendsInterface().QueryFriends(queryFriendsOptions, null, (QueryFriendsCallbackInfo queryFriendsCallbackInfo) =>
            {
                Debug.WriteLine($"QueryFriends {queryFriendsCallbackInfo.ResultCode}");

                if (queryFriendsCallbackInfo.ResultCode == Result.Success)
                {
                    var getFriendsCountOptions = new GetFriendsCountOptions()
                    {
                        LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId)
                    };
                    var friendsCount = App.Settings.PlatformInterface.GetFriendsInterface().GetFriendsCount(getFriendsCountOptions);

                    for (int i = 0; i < friendsCount; i++)
                    {
                        var getFriendAtIndexOptions = new GetFriendAtIndexOptions()
                        {
                            LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId),
                            Index = i
                        };
                        var friend = App.Settings.PlatformInterface.GetFriendsInterface().GetFriendAtIndex(getFriendAtIndexOptions);

                        if (friend != null)
                        {
                            var getStatusOptions = new GetStatusOptions()
                            {
                                LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId),
                                TargetUserId = friend
                            };
                            var friendStatus = App.Settings.PlatformInterface.GetFriendsInterface().GetStatus(getStatusOptions);

                            ViewModelLocator.Friends.Friends.Add(new Friend()
                            {
                                EpicAccountId = friend,
                                FriendsStatus = friendStatus
                            });
                        }
                    }

                    ViewModelLocator.Friends.FriendsSubscribeUpdates.RaiseCanExecuteChanged();
                    ViewModelLocator.Friends.FriendsUnsubscribeUpdates.RaiseCanExecuteChanged();
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void SubscribeUpdates()
        {
            var addNotifyFriendsUpdateOptions = new AddNotifyFriendsUpdateOptions();

            ViewModelLocator.Main.StatusBarText = $"Adding notification for friends updates...";

            ViewModelLocator.Friends.NotificationId = App.Settings.PlatformInterface.GetFriendsInterface().AddNotifyFriendsUpdate(addNotifyFriendsUpdateOptions, null, (OnFriendsUpdateInfo onFriendsUpdateInfo) =>
            {
                Debug.WriteLine($"OnFriendsUpdate: {onFriendsUpdateInfo.TargetUserId}");

                ViewModelLocator.Friends.Friends.SingleOrDefault(f => f.EpicAccountId == onFriendsUpdateInfo.TargetUserId).FriendsStatus = onFriendsUpdateInfo.CurrentStatus;
            });

            ViewModelLocator.Friends.FriendsSubscribeUpdates.RaiseCanExecuteChanged();
            ViewModelLocator.Friends.FriendsUnsubscribeUpdates.RaiseCanExecuteChanged();

            ViewModelLocator.Main.StatusBarText = string.Empty;
        }

        public static void UnsubscribeUpdates()
        {
            ViewModelLocator.Main.StatusBarText = $"Removing notification for friends updates...";

            App.Settings.PlatformInterface.GetFriendsInterface().RemoveNotifyFriendsUpdate(ViewModelLocator.Friends.NotificationId);
            ViewModelLocator.Friends.NotificationId = 0;

            ViewModelLocator.Friends.FriendsSubscribeUpdates.RaiseCanExecuteChanged();
            ViewModelLocator.Friends.FriendsUnsubscribeUpdates.RaiseCanExecuteChanged();

            ViewModelLocator.Main.StatusBarText = string.Empty;
        }
    }
}
