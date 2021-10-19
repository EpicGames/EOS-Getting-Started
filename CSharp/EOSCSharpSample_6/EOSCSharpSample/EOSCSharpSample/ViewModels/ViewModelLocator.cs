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

        private static FriendsViewModel _friends;
        public static FriendsViewModel Friends
        {
            get { return _friends ??= new FriendsViewModel(); }
        }

        public static void RaiseAuthCanExecuteChanged()
        {
            Main.AuthLogin.RaiseCanExecuteChanged();
            Main.AuthLogout.RaiseCanExecuteChanged();

            Presence.PresenceQuery.RaiseCanExecuteChanged();

            Friends.FriendsQuery.RaiseCanExecuteChanged();
        }
    }
}
