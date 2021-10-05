// Copyright Epic Games, Inc. All Rights Reserved.

namespace EOSCSharpSample.ViewModels
{
    public static class ViewModelLocator
    {
        private static MainViewModel _main;
        public static MainViewModel Main
        {
            get { return _main ??= new MainViewModel(); }
        }

        public static void RaiseAuthCanExecuteChanged()
        {
            Main.AuthLogin.RaiseCanExecuteChanged();
            Main.AuthLogout.RaiseCanExecuteChanged();
        }
    }
}
