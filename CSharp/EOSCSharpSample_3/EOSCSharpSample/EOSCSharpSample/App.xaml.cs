// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Windows;

namespace EOSCSharpSample
{
    public partial class App : Application
    {
        public static ApplicationSettings Settings { get; set; }

        protected override void OnStartup(StartupEventArgs e)
        {
            // Get command line arguments (if any) to overwrite default settings
            var commandLineArgsDict = new Dictionary<string, string>();
            for (int index = 0; index < e.Args.Length; index += 2)
            {
                commandLineArgsDict.Add(e.Args[index], e.Args[index + 1]);
            }

            Settings = new ApplicationSettings();
            Settings.Initialize(commandLineArgsDict);

            base.OnStartup(e);
        }
    }
}
