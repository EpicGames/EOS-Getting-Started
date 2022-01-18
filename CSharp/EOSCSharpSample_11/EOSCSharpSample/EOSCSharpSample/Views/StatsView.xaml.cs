// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using System.Windows.Controls;

namespace EOSCSharpSample.Views
{
    public partial class StatsView : UserControl
    {
        public StatsViewModel ViewModel { get { return ViewModelLocator.Stats; } }

        public StatsView()
        {
            InitializeComponent();
            DataContext = ViewModel;
        }
    }
}
