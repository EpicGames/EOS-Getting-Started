// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.TitleStorage;
using System.Diagnostics;
using System.IO;

namespace EOSCSharpSample.Services
{
    public static class TitleStorageService
    {
        public static void QueryFile(string fileName)
        {
            var queryFileOptions = new QueryFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Filename = fileName
            };

            ViewModelLocator.Main.StatusBarText = $"Querying title storage file <{queryFileOptions.Filename}>...";

            App.Settings.PlatformInterface.GetTitleStorageInterface().QueryFile(queryFileOptions, null, (QueryFileCallbackInfo queryFileCallbackInfo) =>
            {
                Debug.WriteLine($"QueryFile {queryFileCallbackInfo.ResultCode}");

                if (queryFileCallbackInfo.ResultCode == Result.Success)
                {
                    var copyFileMetadataByFilenameOptions = new CopyFileMetadataByFilenameOptions()
                    {
                        LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                        Filename = fileName
                    };
                    var result = App.Settings.PlatformInterface.GetTitleStorageInterface().CopyFileMetadataByFilename(copyFileMetadataByFilenameOptions, out var metadata);

                    if (result == Result.Success)
                    {
                        ViewModelLocator.TitleStorage.TitleStorageFiles.Add(metadata);
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void QueryFileList(string tag)
        {
            var queryFileListOptions = new QueryFileListOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                ListOfTags = new string[] { tag }
            };

            ViewModelLocator.Main.StatusBarText = $"Querying title storage files by tag <{tag}>...";

            App.Settings.PlatformInterface.GetTitleStorageInterface().QueryFileList(queryFileListOptions, null, (QueryFileListCallbackInfo queryFileListCallbackInfo) =>
            {
                Debug.WriteLine($"QueryFileList {queryFileListCallbackInfo.ResultCode}");

                if (queryFileListCallbackInfo.ResultCode == Result.Success)
                {
                    for (uint i = 0; i < queryFileListCallbackInfo.FileCount; i++)
                    {
                        var copyFileMetadataAtIndexOptions = new CopyFileMetadataAtIndexOptions()
                        {
                            Index = i,
                            LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
                        };
                        var result = App.Settings.PlatformInterface.GetTitleStorageInterface().CopyFileMetadataAtIndex(copyFileMetadataAtIndexOptions, out var metadata);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.TitleStorage.TitleStorageFiles.Add(metadata);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void ReadFile(FileMetadata fileMetadata)
        {
            var readFileOptions = new ReadFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Filename = fileMetadata.Filename,
                ReadChunkLengthBytes = 4096,
                ReadFileDataCallback = (ReadFileDataCallbackInfo readFileDataCallbackInfo) =>
                {
                    using var fs = new FileStream($"{App.Settings.CacheDirectory}{readFileDataCallbackInfo.Filename}", FileMode.Append, FileAccess.Write);
                    fs.Write(readFileDataCallbackInfo.DataChunk, 0, readFileDataCallbackInfo.DataChunk.Length);
                    return ReadResult.RrContinuereading;
                },
                FileTransferProgressCallback = (FileTransferProgressCallbackInfo fileTransferProgressCallbackInfo) =>
                {
                    var percentComplete = fileTransferProgressCallbackInfo.BytesTransferred / fileTransferProgressCallbackInfo.TotalFileSizeBytes * 100;
                    ViewModelLocator.Main.StatusBarText = $"Downloading file <{fileTransferProgressCallbackInfo.Filename}> ({percentComplete}%)...";
                }
            };

            ViewModelLocator.Main.StatusBarText = $"Downloading file <{readFileOptions.Filename}>...";

            var fileTransferRequest = App.Settings.PlatformInterface.GetTitleStorageInterface().ReadFile(readFileOptions, null, (ReadFileCallbackInfo readFileCallbackInfo) =>
            {
                Debug.WriteLine($"ReadFile {readFileCallbackInfo.ResultCode}");

                if (readFileCallbackInfo.ResultCode == Result.Success)
                {
                    Debug.WriteLine($"Successfully downloaded {readFileCallbackInfo.Filename} to {App.Settings.CacheDirectory}.");
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
            });

            if (fileTransferRequest == null)
            {
                Debug.WriteLine("Error downloading file: bad handle");
                ViewModelLocator.Main.StatusBarText = string.Empty;
            }
        }

    }
}
