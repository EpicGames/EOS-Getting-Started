// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.PlayerDataStorage;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class PlayerDataStorageViewModel : BindableBase
    {
        private ObservableCollection<FileMetadata> _playerDataStorageFiles;
        public ObservableCollection<FileMetadata> PlayerDataStorageFiles
        {
            get { return _playerDataStorageFiles; }
            set { SetProperty(ref _playerDataStorageFiles, value); }
        }

        private FileMetadata _selectedPlayerDataStorageFile;
        public FileMetadata SelectedPlayerDataStorageFile
        {
            get { return _selectedPlayerDataStorageFile; }
            set { SetProperty(ref _selectedPlayerDataStorageFile, value); }
        }

        public PlayerDataStorageQueryFileListCommand PlayerDataStorageQueryFileList { get; set; }
        public PlayerDataStorageWriteFileCommand PlayerDataStorageWriteFile { get; set; }
        public PlayerDataStorageReadFileCommand PlayerDataStorageReadFile { get; set; }
        public PlayerDataStorageDuplicateFileCommand PlayerDataStorageDuplicateFile { get; set; }
        public PlayerDataStorageDeleteFileCommand PlayerDataStorageDeleteFile { get; set; }

        public PlayerDataStorageViewModel()
        {
            PlayerDataStorageQueryFileList = new PlayerDataStorageQueryFileListCommand();
            PlayerDataStorageWriteFile = new PlayerDataStorageWriteFileCommand();
            PlayerDataStorageReadFile = new PlayerDataStorageReadFileCommand();
            PlayerDataStorageDuplicateFile = new PlayerDataStorageDuplicateFileCommand();
            PlayerDataStorageDeleteFile = new PlayerDataStorageDeleteFileCommand();
        }
    }
}
