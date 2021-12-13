// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;

namespace EOSCSharpSample.ViewModels
{
    public class MainViewModel : BindableBase
    {
        private string _statusBarText;
        public string StatusBarText
        {
            get { return _statusBarText; }
            set { SetProperty(ref _statusBarText, value); }
        }

        private string _accountId;
        public string AccountId
        {
            get { return _accountId; }
            set { SetProperty(ref _accountId, value); }
        }

        private string _displayName;
        public string DisplayName
        {
            get { return _displayName; }
            set { SetProperty(ref _displayName, value); }
        }

        private string _productUserId;
        public string ProductUserId
        {
            get { return _productUserId; }
            set { SetProperty(ref _productUserId, value); }
        }

        private ulong _connectAuthExpirationNotificationId;
        public ulong ConnectAuthExpirationNotificationId
        {
            get { return _connectAuthExpirationNotificationId; }
            set { SetProperty(ref _connectAuthExpirationNotificationId, value); }
        }

        private ulong _connectLoginStatusChangedNotificationId;
        public ulong ConnectLoginStatusChangedNotificationId
        {
            get { return _connectLoginStatusChangedNotificationId; }
            set { SetProperty(ref _connectLoginStatusChangedNotificationId, value); }
        }

        public AuthLoginCommand AuthLogin { get; set; }
        public AuthLogoutCommand AuthLogout { get; set; }
        public ConnectLoginCommand ConnectLogin { get; set; }

        public MainViewModel()
        {
            AuthLogin = new AuthLoginCommand();
            AuthLogout = new AuthLogoutCommand();
            ConnectLogin = new ConnectLoginCommand();
        }
    }
}
