// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.Stats;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class StatsViewModel : BindableBase
    {
        private int _clicks;
        public int Clicks
        {
            get { return _clicks; }
            set { SetProperty(ref _clicks, value); }
        }

        private ObservableCollection<Stat> _stats;
        public ObservableCollection<Stat> Stats
        {
            get { return _stats; }
            set { SetProperty(ref _stats, value); }
        }

        public StatsClickCommand StatsClick { get; set; }
        public StatsIngestCommand StatsIngest { get; set; }
        public StatsQueryCommand StatsQuery { get; set; }

        public StatsViewModel()
        {
            StatsClick = new StatsClickCommand();
            StatsIngest = new StatsIngestCommand();
            StatsQuery = new StatsQueryCommand();
        }
    }
}
