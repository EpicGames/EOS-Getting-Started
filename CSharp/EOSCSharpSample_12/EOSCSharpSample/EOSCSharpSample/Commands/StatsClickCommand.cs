// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.ViewModels;

namespace EOSCSharpSample.Commands
{
    public class StatsClickCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Stats.Clicks++;
        }
    }
}
