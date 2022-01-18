// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.Stats;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class StatsQueryCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Stats.Stats = new ObservableCollection<Stat>();
            StatsService.Query();
        }
    }
}
