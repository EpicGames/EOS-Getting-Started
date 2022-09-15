// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Connect;
using System.Diagnostics;
using System.Windows;

namespace EOSCSharpSample.Services
{
    public static class ConnectService
    {
        public static void ConnectLogin()
        {
            ViewModelLocator.Main.StatusBarText = "Getting auth interface...";

            var authInterface = App.Settings.PlatformInterface.GetAuthInterface();
            if (authInterface == null)
            {
                Debug.WriteLine("Failed to get auth interface");
                ViewModelLocator.Main.StatusBarText = string.Empty;
                return;
            }
            var copyIdTokenOptions = new Epic.OnlineServices.Auth.CopyIdTokenOptions()
            {
                AccountId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId)
            };

            var result = authInterface.CopyIdToken(ref copyIdTokenOptions, out var userAuthToken);

            if (result == Result.Success)
            {
                var connectInterface = App.Settings.PlatformInterface.GetConnectInterface();
                if (connectInterface == null)
                {
                    Debug.WriteLine("Failed to get connect interface");
                    return;
                }

                var loginOptions = new LoginOptions()
                {
                    Credentials = new Credentials()
                    {
                        Type = ExternalCredentialType.EpicIdToken,
                        Token = userAuthToken.Value.JsonWebToken
                    }
                };

                ViewModelLocator.Main.StatusBarText = "Requesting user login...";

                // Ensure platform tick is called on an interval, or the following call will never callback.
                connectInterface.Login(ref loginOptions, null, (ref Epic.OnlineServices.Connect.LoginCallbackInfo loginCallbackInfo) =>
                {
                    Debug.WriteLine($"Connect login {loginCallbackInfo.ResultCode}");

                    if (loginCallbackInfo.ResultCode == Result.Success)
                    {
                        ViewModelLocator.Main.StatusBarText = "Connect login successful.";

                        var notifyAuthExpirationOptions = new AddNotifyAuthExpirationOptions();
                        var notifyLoginStatusChangedOptions = new AddNotifyLoginStatusChangedOptions();

                        ViewModelLocator.Main.ConnectAuthExpirationNotificationId = connectInterface.AddNotifyAuthExpiration(ref notifyAuthExpirationOptions, null, AuthExpirationCallback);
                        ViewModelLocator.Main.ConnectLoginStatusChangedNotificationId = connectInterface.AddNotifyLoginStatusChanged(ref notifyLoginStatusChangedOptions, null, LoginStatusChangedCallback);

                        ViewModelLocator.Main.ProductUserId = loginCallbackInfo.LocalUserId.ToString();
                    }
                    else if (loginCallbackInfo.ResultCode == Result.InvalidUser)
                    {
                        ViewModelLocator.Main.StatusBarText = "Connect login failed: " + loginCallbackInfo.ResultCode;

                        var loginWithDifferentCredentials = MessageBox.Show("User not found. Log in with different credentials?", "Invalid User", MessageBoxButton.YesNo, MessageBoxImage.Question);
                        if (loginWithDifferentCredentials == MessageBoxResult.No)
                        {
                            var createUserOptions = new CreateUserOptions()
                            {
                                ContinuanceToken = loginCallbackInfo.ContinuanceToken
                            };

                            connectInterface.CreateUser(ref createUserOptions, null, (ref CreateUserCallbackInfo createUserCallbackInfo) =>
                            {
                                if (createUserCallbackInfo.ResultCode == Result.Success)
                                {
                                    ViewModelLocator.Main.StatusBarText = "User successfully created.";

                                    var notifyAuthExpirationOptions = new AddNotifyAuthExpirationOptions();
                                    var notifyLoginStatusChangedOptions = new AddNotifyLoginStatusChangedOptions();

                                    ViewModelLocator.Main.ConnectAuthExpirationNotificationId = connectInterface.AddNotifyAuthExpiration(ref notifyAuthExpirationOptions, null, AuthExpirationCallback);
                                    ViewModelLocator.Main.ConnectLoginStatusChangedNotificationId = connectInterface.AddNotifyLoginStatusChanged(ref notifyLoginStatusChangedOptions, null, LoginStatusChangedCallback);

                                    ViewModelLocator.Main.ProductUserId = createUserCallbackInfo.LocalUserId.ToString();
                                }
                                else if (Common.IsOperationComplete(createUserCallbackInfo.ResultCode))
                                {
                                    Debug.WriteLine("User creation failed: " + createUserCallbackInfo.ResultCode);
                                }

                                ViewModelLocator.Main.StatusBarText = string.Empty;
                                ViewModelLocator.RaiseConnectCanExecuteChanged();
                            });
                        }
                        else
                        {
                            // Prompt for login with different credentials
                        }
                    }
                    else if (Common.IsOperationComplete(loginCallbackInfo.ResultCode))
                    {
                        Debug.WriteLine("Connect login failed: " + loginCallbackInfo.ResultCode);
                    }

                    ViewModelLocator.Main.StatusBarText = string.Empty;
                    ViewModelLocator.RaiseConnectCanExecuteChanged();
                });
            }
            else if (Common.IsOperationComplete(result))
            {
                Debug.WriteLine("CopyIdToken failed: " + result);
                ViewModelLocator.Main.StatusBarText = string.Empty;
            }
        }

        private static void AuthExpirationCallback(ref AuthExpirationCallbackInfo data)
        {
            // Handle 10-minute warning prior to token expiration by calling Connect.Login()
        }
        private static void LoginStatusChangedCallback(ref LoginStatusChangedCallbackInfo data)
        {
            switch (data.CurrentStatus)
            {
                case LoginStatus.NotLoggedIn:
                    if (data.PreviousStatus == LoginStatus.LoggedIn)
                    {
                        // Handle token expiration
                    }
                    break;
                case LoginStatus.UsingLocalProfile:
                    break;
                case LoginStatus.LoggedIn:
                    break;
            }
        }

        public static void RemoveNotifications()
        {
            App.Settings.PlatformInterface.GetConnectInterface().RemoveNotifyAuthExpiration(ViewModelLocator.Main.ConnectAuthExpirationNotificationId);
            App.Settings.PlatformInterface.GetConnectInterface().RemoveNotifyLoginStatusChanged(ViewModelLocator.Main.ConnectLoginStatusChangedNotificationId);
        }
    }
}
