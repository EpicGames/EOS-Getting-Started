// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices;
using Epic.OnlineServices.PlayerDataStorage;
using Microsoft.Win32;
using System.Diagnostics;
using System.IO;

namespace EOSCSharpSample.Services
{
    public static class PlayerDataStorageService
    {
        public static void QueryFileList()
        {
            var queryFileListOptions = new QueryFileListOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId)
            };

            ViewModelLocator.Main.StatusBarText = "Querying player data storage file list...";

            App.Settings.PlatformInterface.GetPlayerDataStorageInterface().QueryFileList(ref queryFileListOptions, null, (ref QueryFileListCallbackInfo queryFileListCallbackInfo) =>
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
                        var result = App.Settings.PlatformInterface.GetPlayerDataStorageInterface().CopyFileMetadataAtIndex(ref copyFileMetadataAtIndexOptions, out var metadata);

                        if (result == Result.Success)
                        {
                            ViewModelLocator.PlayerDataStorage.PlayerDataStorageFiles.Add(metadata.Value);
                        }
                    }
                }

                ViewModelLocator.Main.StatusBarText = string.Empty;
            });
        }

        public static void WriteFile(OpenFileDialog openFileDialog)
        {
            var bytesWritten = 0;

            var writeFileOptions = new WriteFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Filename = openFileDialog.SafeFileName,
                ChunkLengthBytes = 10485760,
                WriteFileDataCallback = (ref WriteFileDataCallbackInfo writeFileDataCallbackInfo, out ArraySegment<byte> buffer) =>
                {
                    using var fs = new FileStream($"{openFileDialog.FileName}", FileMode.Open, FileAccess.Read);
                    if (fs.Length > bytesWritten)
                    {
                        var readBytes = new byte[System.Math.Min(writeFileDataCallbackInfo.DataBufferLengthBytes, fs.Length)];
                        fs.Seek(bytesWritten, SeekOrigin.Begin);
                        bytesWritten += fs.Read(readBytes, 0, (int)System.Math.Min(writeFileDataCallbackInfo.DataBufferLengthBytes, fs.Length));
                        buffer = readBytes;
                    }
                    else
                    {
                        buffer = new byte[0];
                        return WriteResult.CompleteRequest;
                    }
                    return WriteResult.ContinueWriting;
                },
                FileTransferProgressCallback = (ref FileTransferProgressCallbackInfo fileTransferProgressCallbackInfo) =>
                {
                    var percentComplete = (double)fileTransferProgressCallbackInfo.BytesTransferred / (double)fileTransferProgressCallbackInfo.TotalFileSizeBytes * 100;
                    ViewModelLocator.Main.StatusBarText = $"Downloading file <{fileTransferProgressCallbackInfo.Filename}> ({System.Math.Ceiling(percentComplete)}%)...";
                }
            };

            ViewModelLocator.Main.StatusBarText = $"Uploading file <{writeFileOptions.Filename}> (creating buffer)...";

            var fileTransferRequest = App.Settings.PlatformInterface.GetPlayerDataStorageInterface().WriteFile(ref writeFileOptions, null, (ref WriteFileCallbackInfo writeFileCallbackInfo) =>
            {
                Debug.WriteLine($"WriteFile {writeFileCallbackInfo.ResultCode}");

                if (writeFileCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.PlayerDataStorage.PlayerDataStorageQueryFileList.Execute(null);
                    Debug.WriteLine($"Successfully uploaded {writeFileCallbackInfo.Filename}.");
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
                else
                {
                    Debug.WriteLine($"Error uploading {writeFileCallbackInfo.Filename}: {writeFileCallbackInfo.ResultCode}.");
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
            });

            if (fileTransferRequest == null)
            {
                Debug.WriteLine("Error uploading file: bad handle");
                ViewModelLocator.Main.StatusBarText = string.Empty;
            }
        }

        public static void ReadFile(FileMetadata fileMetadata)
        {
            var readFileOptions = new ReadFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Filename = fileMetadata.Filename,
                ReadChunkLengthBytes = 1048576,
                ReadFileDataCallback = (ref ReadFileDataCallbackInfo readFileDataCallbackInfo) =>
                {
                    using var fs = new FileStream($"{App.Settings.CacheDirectory}{readFileDataCallbackInfo.Filename}", FileMode.Append, FileAccess.Write);
                    fs.Write(readFileDataCallbackInfo.DataChunk.ToArray(), 0, readFileDataCallbackInfo.DataChunk.Count);
                    return ReadResult.ContinueReading;
                },
                FileTransferProgressCallback = (ref FileTransferProgressCallbackInfo fileTransferProgressCallbackInfo) =>
                {
                    var percentComplete = (double)fileTransferProgressCallbackInfo.BytesTransferred / (double)fileTransferProgressCallbackInfo.TotalFileSizeBytes * 100;
                    ViewModelLocator.Main.StatusBarText = $"Downloading file <{fileTransferProgressCallbackInfo.Filename}> ({System.Math.Ceiling(percentComplete)}%)...";
                }
            };

            ViewModelLocator.Main.StatusBarText = $"Downloading file <{readFileOptions.Filename}> (creating buffer)...";

            var fileTransferRequest = App.Settings.PlatformInterface.GetPlayerDataStorageInterface().ReadFile(ref readFileOptions, null, (ref ReadFileCallbackInfo readFileCallbackInfo) =>
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

        public static void DuplicateFile(FileMetadata fileMetadata)
        {
            var duplicateFileOptions = new DuplicateFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                SourceFilename = fileMetadata.Filename,
                DestinationFilename = $"{fileMetadata.Filename}_(copy)"
            };

            ViewModelLocator.Main.StatusBarText = $"Copying <{duplicateFileOptions.SourceFilename}> as <{duplicateFileOptions.DestinationFilename}>...";

            App.Settings.PlatformInterface.GetPlayerDataStorageInterface().DuplicateFile(ref duplicateFileOptions, null, (ref DuplicateFileCallbackInfo duplicateFileCallbackInfo) =>
            {
                Debug.WriteLine($"DuplicateFile {duplicateFileCallbackInfo.ResultCode}");

                if (duplicateFileCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.PlayerDataStorage.PlayerDataStorageQueryFileList.Execute(null);
                    ViewModelLocator.Main.StatusBarText = "Successfully copied file.";
                }
                else
                {
                    Debug.WriteLine("Copying file failed: " + duplicateFileCallbackInfo.ResultCode);
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
            });
        }

        public static void DeleteFile(FileMetadata fileMetadata)
        {
            var deleteFileOptions = new DeleteFileOptions()
            {
                LocalUserId = ProductUserId.FromString(ViewModelLocator.Main.ProductUserId),
                Filename = fileMetadata.Filename
            };

            ViewModelLocator.Main.StatusBarText = $"Deleting <{deleteFileOptions.Filename}>...";

            App.Settings.PlatformInterface.GetPlayerDataStorageInterface().DeleteFile(ref deleteFileOptions, null, (ref DeleteFileCallbackInfo deleteFileCallbackInfo) =>
            {
                Debug.WriteLine($"DeleteFile {deleteFileCallbackInfo.ResultCode}");

                if (deleteFileCallbackInfo.ResultCode == Result.Success)
                {
                    ViewModelLocator.PlayerDataStorage.PlayerDataStorageQueryFileList.Execute(null);
                    ViewModelLocator.Main.StatusBarText = "Successfully deleted file.";
                }
                else
                {
                    Debug.WriteLine("Deleting file failed: " + deleteFileCallbackInfo.ResultCode);
                    ViewModelLocator.Main.StatusBarText = string.Empty;
                }
            });
        }
    }
}
