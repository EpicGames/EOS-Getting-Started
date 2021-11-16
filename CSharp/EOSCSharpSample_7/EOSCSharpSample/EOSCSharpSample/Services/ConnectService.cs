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

            var result = authInterface.CopyIdToken(copyIdTokenOptions, out var userAuthToken);

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
                        Token = userAuthToken.JsonWebToken
                    }
                };

                ViewModelLocator.Main.StatusBarText = "Requesting user login...";

                // Ensure platform tick is called on an interval, or the following call will never callback.
                connectInterface.Login(loginOptions, null, (Epic.OnlineServices.Connect.LoginCallbackInfo loginCallbackInfo) =>
                {
                    Debug.WriteLine($"Connect login {loginCallbackInfo.ResultCode}");

                    if (loginCallbackInfo.ResultCode == Result.Success)
                    {
                        ViewModelLocator.Main.StatusBarText = "Connect login successful.";

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

                            connectInterface.CreateUser(createUserOptions, null, (CreateUserCallbackInfo createUserCallbackInfo) =>
                            {
                                if (createUserCallbackInfo.ResultCode == Result.Success)
                                {
                                    ViewModelLocator.Main.StatusBarText = "User successfully created.";

                                    ViewModelLocator.Main.ProductUserId = createUserCallbackInfo.LocalUserId.ToString();
                                }
                                else if (Common.IsOperationComplete(loginCallbackInfo.ResultCode))
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
                    else if (Common.IsOperationComplete(result))
                    {
                        Debug.WriteLine("Connect login failed: " + result);
                    }

                    ViewModelLocator.Main.StatusBarText = string.Empty;
                    ViewModelLocator.RaiseConnectCanExecuteChanged();
                });
            }
        }
    }
}
