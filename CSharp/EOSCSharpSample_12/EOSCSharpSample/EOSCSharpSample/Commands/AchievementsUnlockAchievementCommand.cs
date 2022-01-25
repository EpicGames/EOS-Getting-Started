// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Helpers;
using EOSCSharpSample.Services;
using EOSCSharpSample.ViewModels;

namespace EOSCSharpSample.Commands
{
    public class AchievementsUnlockAchievementCommand : CommandBase
    {
        public override bool CanExecute(object parameter)
        {
            return ViewModelLocator.Achievements.SelectedAchievement != null;
        }

        public override void Execute(object parameter)
        {
            AchievementsService.UnlockAchievement(ViewModelLocator.Achievements.SelectedAchievement);
        }
    }
}
