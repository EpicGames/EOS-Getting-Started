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

        private static TitleStorageViewModel _titleStorage;
        public static TitleStorageViewModel TitleStorage
        {
            get { return _titleStorage ??= new TitleStorageViewModel(); }
        }

        private static PlayerDataStorageViewModel _playerDataStorage;
        public static PlayerDataStorageViewModel PlayerDataStorage
        {
            get { return _playerDataStorage ??= new PlayerDataStorageViewModel(); }
        }

        private static StatsViewModel _statsViewModel;
        public static StatsViewModel Stats
        {
            get { return _statsViewModel ??= new StatsViewModel(); }
        }

        public static void RaiseAuthCanExecuteChanged()
        {
            Main.AuthLogin.RaiseCanExecuteChanged();
            Main.AuthLogout.RaiseCanExecuteChanged();

            Main.ConnectLogin.RaiseCanExecuteChanged();

            Presence.PresenceQuery.RaiseCanExecuteChanged();

            Friends.FriendsQuery.RaiseCanExecuteChanged();
        }

        public static void RaiseConnectCanExecuteChanged()
        {
            Main.ConnectLogin.RaiseCanExecuteChanged();

            TitleStorage.TitleStorageQueryFile.RaiseCanExecuteChanged();
            TitleStorage.TitleStorageQueryFileList.RaiseCanExecuteChanged();

            PlayerDataStorage.PlayerDataStorageQueryFileList.RaiseCanExecuteChanged();
            PlayerDataStorage.PlayerDataStorageWriteFile.RaiseCanExecuteChanged();

            Stats.StatsClick.RaiseCanExecuteChanged();
            Stats.StatsIngest.RaiseCanExecuteChanged();
            Stats.StatsQuery.RaiseCanExecuteChanged();
        }
    }
}
