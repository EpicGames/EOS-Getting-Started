// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class PresenceView : UserControl
    {
        public PresenceViewModel ViewModel { get { return ViewModelLocator.Presence; } }

        public PresenceView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }
    }
}
