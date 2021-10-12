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

        private static PresenceViewModel _presence;
        public static PresenceViewModel Presence
        {
            get { return _presence ??= new PresenceViewModel(); }
        }

        public static void RaiseAuthCanExecuteChanged()
        {
            Main.AuthLogin.RaiseCanExecuteChanged();
            Main.AuthLogout.RaiseCanExecuteChanged();

            Presence.PresenceQuery.RaiseCanExecuteChanged();
        }
    }
}
