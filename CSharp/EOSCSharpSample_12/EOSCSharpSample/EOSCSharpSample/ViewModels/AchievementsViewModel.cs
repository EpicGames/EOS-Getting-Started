// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.Commands;
using EOSCSharpSample.Helpers;
using Epic.OnlineServices.Achievements;
using System.Collections.ObjectModel;

namespace EOSCSharpSample.ViewModels
{
    public class AchievementsViewModel : BindableBase
    {
        private ObservableCollection<DefinitionV2> _achievements;
        public ObservableCollection<DefinitionV2> Achievements
        {
            get { return _achievements; }
            set { SetProperty(ref _achievements, value); }
        }

        private DefinitionV2 _selectedAchievement;
        public DefinitionV2 SelectedAchievement
        {
            get { return _selectedAchievement; }
            set { SetProperty(ref _selectedAchievement, value); }
        }

        private ObservableCollection<PlayerAchievement> _playerAchievements;
        public ObservableCollection<PlayerAchievement> PlayerAchievements
        {
            get { return _playerAchievements; }
            set { SetProperty(ref _playerAchievements, value); }
        }

        public AchievementsQueryDefinitionsCommand AchievementsQueryDefinitions { get; set; }
        public AchievementsQueryPlayerAchievementsCommand AchievementsQueryPlayerAchievements { get; set; }
        public AchievementsUnlockAchievementsCommand AchievementsUnlockAchievements { get; set; }

        public AchievementsViewModel()
        {
            AchievementsQueryDefinitions = new AchievementsQueryDefinitionsCommand();
            AchievementsQueryPlayerAchievements = new AchievementsQueryPlayerAchievementsCommand();
            AchievementsUnlockAchievements = new AchievementsUnlockAchievementsCommand();
        }
    }
}
