// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.TitleStorage;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class TitleStorageViewModel : BindableBase
    {
        private ObservableCollection<FileMetadata> _titleStorageFiles;
        public ObservableCollection<FileMetadata> TitleStorageFiles
        {
            get { return _titleStorageFiles; }
            set { SetProperty(ref _titleStorageFiles, value); }
        }

        private FileMetadata _selectedTitleStorageFile;
        public FileMetadata SelectedTitleStorageFile
        {
            get { return _selectedTitleStorageFile; }
            set { SetProperty(ref _selectedTitleStorageFile, value); }
        }

        private string _titleStorageFileName;
        public string TitleStorageFileName
        {
            get { return _titleStorageFileName; }
            set { SetProperty(ref _titleStorageFileName, value); }
        }

        private string _titleStorageTag;
        public string TitleStorageTag
        {
            get { return _titleStorageTag; }
            set { SetProperty(ref _titleStorageTag, value); }
        }

        public TitleStorageQueryFileCommand TitleStorageQueryFile { get; set; }
        public TitleStorageQueryFileListCommand TitleStorageQueryFileList { get; set; }
        public TitleStorageReadFileCommand TitleStorageReadFile { get; set; }

        public TitleStorageViewModel()
        {
            TitleStorageQueryFile = new TitleStorageQueryFileCommand();
            TitleStorageQueryFileList = new TitleStorageQueryFileListCommand();
            TitleStorageReadFile = new TitleStorageReadFileCommand();
        }
    }
}
