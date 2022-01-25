// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.Achievements;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.Commands
{
    public class AchievementsQueryDefinitionsCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return !string.IsNullOrWhiteSpace(ViewModelLocator.Main.ProductUserId);
        }

        public override void Execute(object parameter)
        {
            ViewModelLocator.Achievements.Achievements = new ObservableCollection<DefinitionV2>();
            AchievementsService.QueryDefinitions();
        }
    }
}
