// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Auth;
using Epic.OnlineServices.UserInfo;
using System.Diagnostics;

namespace EOSCSharpSample.Services
{
    public static class AuthService
    {
        public static void AuthLogin()
        {
            ViewModelLocator.Main.StatusBarText = "Getting auth interface...";

            var authInterface = App.Settings.PlatformInterface.GetAuthInterface();
            if (authInterface == null)
            {
                Debug.WriteLine("Failed to get auth interface");
                ViewModelLocator.Main.StatusBarText = string.Empty;
                return;
            }

            var loginOptions = new LoginOptions()
            {
                Credentials = new Credentials()
                {
                    Type = App.Settings.LoginCredentialType,
                    Id = App.Settings.Id,
                    Token = App.Settings.Token,
                    ExternalType = App.Settings.ExternalCredentialType
                },
                ScopeFlags = App.Settings.ScopeFlags
            };

            ViewModelLocator.Main.StatusBarText = "Requesting user login...";

            // Ensure platform tick is called on an interval, or the following call will never callback.
            authInterface.Login(loginOptions, null, (LoginCallbackInfo loginCallbackInfo) =>
            {
                Debug.WriteLine($"Auth login {loginCallbackInfo.ResultCode}");

                if (loginCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.Main.StatusBarText = "Auth login successful.";

                    ViewModelLocator.Main.AccountId = loginCallbackInfo.LocalUserId.ToString();

                    var userInfoInterface = App.Settings.PlatformInterface.GetUserInfoInterface();
                    if (userInfoInterface == null)
                    {
                        Debug.WriteLine("Failed to get user info interface");
                        return;
                    }

                    // https://dev.epicgames.com/docs/services/en-US/Interfaces/UserInfo/index.html#retrievinguserinfobyaccountidentifier
                    // The first step in dealing with user info is to call EOS_UserInfo_QueryUserInfo with an EOS_UserInfo_QueryUserInfoOptions data structure.
                    // This will download the most up-to-date version of a user's information into the local cache.
                    // To perform the EOS_UserInfo_QueryUserInfo call, create and initialize an EOS_UserInfo_QueryUserInfoOptions
                    var queryUserInfoOptions = new QueryUserInfoOptions()
                    {
                        LocalUserId = loginCallbackInfo.LocalUserId,
                        TargetUserId = loginCallbackInfo.LocalUserId
                    };

                    ViewModelLocator.Main.StatusBarText = "Getting user info...";

                    userInfoInterface.QueryUserInfo(queryUserInfoOptions, null, (QueryUserInfoCallbackInfo queryUserInfoCallbackInfo) =>
                    {
                        Debug.WriteLine($"QueryUserInfo {queryUserInfoCallbackInfo.ResultCode}");

                        if (queryUserInfoCallbackInfo.ResultCode == Result.Success)
                        {
                            ViewModelLocator.Main.StatusBarText = "User info retrieved.";

                            // https://dev.epicgames.com/docs/services/en-US/Interfaces/UserInfo/index.html#examininguserinformation
                            // Once you have retrieved information about a specific user from the online service, you can request a copy of that data with the EOS_UserInfo_CopyUserInfo function.
                            // This function requires an EOS_UserInfo_CopyUserInfoOptions structure 
                            var copyUserInfoOptions = new CopyUserInfoOptions()
                            {
                                LocalUserId = queryUserInfoCallbackInfo.LocalUserId,
                                TargetUserId = queryUserInfoCallbackInfo.TargetUserId
                            };

                            var result = userInfoInterface.CopyUserInfo(copyUserInfoOptions, out var userInfoData);
                            Debug.WriteLine($"CopyUserInfo: {result}");

                            if (userInfoData != null)
                            {
                                ViewModelLocator.Main.DisplayName = userInfoData.DisplayName;
                            }

                            ViewModelLocator.Main.StatusBarText = string.Empty;
                            ViewModelLocator.RaiseAuthCanExecuteChanged();
                        }
                    });
                }
                else if (Common.IsOperationComplete(loginCallbackInfo.ResultCode))
                {
                    Debug.WriteLine("Login failed: " + loginCallbackInfo.ResultCode);
                }
            });
        }

        public static void AuthLogout()
        {
            // https://dev.epicgames.com/docs/services/en-US/Interfaces/Auth/index.html#logout
            // To log out, make a call to EOS_Auth_Logout with an EOS_Auth_LogoutOptions data structure. When the operation completes, your callback EOS_Auth_OnLogoutCallback will run.
            var logoutOptions = new LogoutOptions()
            {
                LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId)
            };

            App.Settings.PlatformInterface.GetAuthInterface().Logout(logoutOptions, null, (LogoutCallbackInfo logoutCallbackInfo) =>
            {
                Debug.WriteLine($"Logout {logoutCallbackInfo.ResultCode}");

                if (logoutCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.Main.StatusBarText = "Logout successful.";

                    // If the EOS_LCT_PersistentAuth login type has been used, call the function EOS_Auth_DeletePersistentAuth to revoke the cached authentication as well.
                    // This permanently erases the local user login on PC Desktop and Mobile.
                    var deletePersistentAuthOptions = new DeletePersistentAuthOptions();
                    App.Settings.PlatformInterface.GetAuthInterface().DeletePersistentAuth(deletePersistentAuthOptions, null, (DeletePersistentAuthCallbackInfo deletePersistentAuthCallbackInfo) =>
                    {
                        Debug.WriteLine($"DeletePersistentAuth {logoutCallbackInfo.ResultCode}");

                        if (logoutCallbackInfo.ResultCode == Result.Success)
                        {
                            ViewModelLocator.Main.StatusBarText = "Persistent auth deleted.";

                            ViewModelLocator.Main.AccountId = string.Empty;
                            ViewModelLocator.Main.DisplayName = string.Empty;

                            ViewModelLocator.Main.ProductUserId = string.Empty;

                            ViewModelLocator.Main.StatusBarText = string.Empty;
                            ViewModelLocator.RaiseAuthCanExecuteChanged();
                        }
                    });
                }
                else if (Common.IsOperationComplete(logoutCallbackInfo.ResultCode))
                {
                    Debug.WriteLine("Logout failed: " + logoutCallbackInfo.ResultCode);
                }
            });
        }
    }
}
