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

        public AuthLoginCommand AuthLogin { get; set; }
        public AuthLogoutCommand AuthLogout { get; set; }

        public MainViewModel()
        {
            AuthLogin = new AuthLoginCommand();
            AuthLogout = new AuthLogoutCommand();
        }
    }
}
