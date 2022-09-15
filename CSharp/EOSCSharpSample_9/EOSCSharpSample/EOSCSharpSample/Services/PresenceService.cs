// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.Presence;
using System.Diagnostics;
using System.Linq;

namespace EOSCSharpSample.Services
{
    public static class PresenceService
    {
        public static void Query(EpicAccountId targetUserId)
        {
            var queryPresenceOptions = new QueryPresenceOptions()
            {
                LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId),
                TargetUserId = targetUserId
            };

            ViewModelLocator.Main.StatusBarText = "Querying user presence...";

            App.Settings.PlatformInterface.GetPresenceInterface().QueryPresence(ref queryPresenceOptions, null, (ref QueryPresenceCallbackInfo queryPresenceCallbackInfo) =>
            {
                Debug.WriteLine($"QueryPresence {queryPresenceCallbackInfo.ResultCode}");

                if (queryPresenceCallbackInfo.ResultCode == Result.Success)
                {
                    Copy(queryPresenceCallbackInfo.LocalUserId, queryPresenceCallbackInfo.TargetUserId);
                }
                else if (Common.IsOperationComplete(queryPresenceCallbackInfo.ResultCode))
                {
                    Debug.WriteLine("Query presence failed: " + queryPresenceCallbackInfo.ResultCode);
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
            });
        }

        public static void Copy(EpicAccountId localUserId, EpicAccountId targetUserId)
        {
            var copyPresenceOptions = new CopyPresenceOptions()
            {
                LocalUserId = localUserId,
                TargetUserId = targetUserId
            };

            var result = App.Settings.PlatformInterface.GetPresenceInterface().CopyPresence(ref copyPresenceOptions, out var info);
            if (result == Result.Success)
            {
                ViewModelLocator.Main.StatusBarText = "User presence retrieved.";

                if (localUserId == targetUserId)
                {
                    ViewModelLocator.Presence.Status = info.Value.Status;
                    ViewModelLocator.Presence.ProductIdText = info.Value.ProductId;
                    ViewModelLocator.Presence.ProductVersionText = info.Value.ProductVersion;
                    ViewModelLocator.Presence.PlatformText = info.Value.Platform;
                    ViewModelLocator.Presence.RichText = info.Value.RichText;
                }
                else
                {
                    ViewModelLocator.Friends.Friends.SingleOrDefault(f => f.EpicAccountId == info.Value.UserId).Status = info.Value.Status;
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
                ViewModelLocator.Presence.PresenceModifyStatus.RaiseCanExecuteChanged();
            }
        }

        public static void ModifyStatus()
        {
            var createPresenceModificationOptions = new CreatePresenceModificationOptions()
            {
                LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId)
            };

            ViewModelLocator.Main.StatusBarText = "Creating presence modification...";

            var result = App.Settings.PlatformInterface.GetPresenceInterface().CreatePresenceModification(ref createPresenceModificationOptions, out var presenceModification);

            Debug.WriteLine($"CreatePresenceModification {result}");

            if (result == Result.Success)
            {
                var setStatusOptions = new PresenceModificationSetStatusOptions()
                {
                    Status = ViewModelLocator.Presence.Status
                };

                result = presenceModification.SetStatus(ref setStatusOptions);
                Debug.WriteLine($"SetStatus {result}");

                var setPresenceOptions = new SetPresenceOptions()
                {
                    LocalUserId = EpicAccountId.FromString(ViewModelLocator.Main.AccountId),
                    PresenceModificationHandle = presenceModification
                };

                ViewModelLocator.Main.StatusBarText = "Setting presence status...";

                App.Settings.PlatformInterface.GetPresenceInterface().SetPresence(ref setPresenceOptions, null, (ref SetPresenceCallbackInfo setPresenceCallbackInfo) =>
                {
                    Debug.WriteLine($"SetPresence {setPresenceCallbackInfo.ResultCode}");
                    if (Common.IsOperationComplete(setPresenceCallbackInfo.ResultCode))
                    {
                        if (presenceModification != null)
                        {
                            presenceModification.Release();
                            presenceModification = null;
                        }

                        ViewModelLocator.Main.StatusBarText = string.Empty;
                    }
                });
            }
            else if (Common.IsOperationComplete(result))
            {
                Debug.WriteLine("Create presence modification failed: " + result);
                ViewModelLocator.Main.StatusBarText = string.Empty;
            }
        }
    }
}
