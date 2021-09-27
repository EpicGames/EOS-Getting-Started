// Copyright Epic Games, Inc. All Rights Reserved.

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

        public MainViewModel()
        {
        }
    }
}
