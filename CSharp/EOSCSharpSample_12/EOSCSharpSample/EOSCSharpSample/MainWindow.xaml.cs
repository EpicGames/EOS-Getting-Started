// Copyright Epic Games, Inc. All Rights Reserved.

using EOSCSharpSample.ViewModels;
using Epic.OnlineServices.Logging;
using Epic.OnlineServices.Platform;
using System;
using System.Diagnostics;
using System.Windows;
using System.Windows.Threading;

namespace EOSCSharpSample
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        // Define a timer to simulate game ticks
        private DispatcherTimer updateTimer;
        private const float updateFrequency = 1 / 30f;

        public MainWindow()
        {
            InitializeComponent();

            DataContext = ViewModelLocator.Main;

            Closing += MainWindow_Closing;
            InitializeApplication();
        }

        // When you no longer need the SDK — generally, at application shutdown time — you can shut it down by calling Epic.OnlineServices.Platform.PlatformInterface.Release
        // to release your Platform Interface instance, and then Epic.OnlineServices.Platform.PlatformInterface.Shutdown to complete the shutdown process
        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            App.Settings.PlatformInterface.Release();
            App.Settings.PlatformInterface = null;

            _ = PlatformInterface.Shutdown();
        }

        private void InitializeApplication()
        {
            var initializeOptions = new InitializeOptions()
            {
                ProductName = "EOSCSharpSample",
                ProductVersion = "1.0.0"
            };

            // The Platform Interface is the entry point to the SDK, and you will need an instance of this interface to use EOS. To create one, call Epic.OnlineServices.Platform.PlatformInterface.
            // Initialize with some basic information about your application
            var result = PlatformInterface.Initialize(initializeOptions);
            Debug.WriteLine($"Initialize: {result}");

            // The SDK outputs useful debugging information through an internal interface.
            // To enable this feature, set up Epic.OnlineServices.Logging.LoggingInterface as early as possible, preferably immediately after initializing the SDK
            _ = LoggingInterface.SetLogLevel(LogCategory.AllCategories, LogLevel.Info);
            _ = LoggingInterface.SetCallback((LogMessage message) => Debug.WriteLine($"[{message.Level}] {message.Category} - {message.Message}"));

            var options = new Options()
            {
                ProductId = App.Settings.ProductId,
                SandboxId = App.Settings.SandboxId,
                ClientCredentials = new ClientCredentials()
                {
                    ClientId = App.Settings.ClientId,
                    ClientSecret = App.Settings.ClientSecret
                },
                DeploymentId = App.Settings.DeploymentId,
                Flags = PlatformFlags.DisableOverlay,
                IsServer = false,

                EncryptionKey = App.Settings.EncryptionKey,
                CacheDirectory = App.Settings.CacheDirectory
            };
            // then call Epic.OnlineServices.Platform.PlatformInterface.Create with the values you have obtained from the Developer Portal to get an Epic.OnlineServices.Platform.PlatformInterface instance
            PlatformInterface platformInterface = PlatformInterface.Create(options);

            if (platformInterface == null)
            {
                Debug.WriteLine($"Failed to create platform. Ensure the relevant settings are set.");
            }

            // Store this instance; you will need it to interact with the SDK
            App.Settings.PlatformInterface = platformInterface;

            // In order for the SDK to operate, you must call Epic.OnlineServices.Platform.PlatformInterface.Tick on your Platform Interface instance regularly
            updateTimer = new DispatcherTimer(DispatcherPriority.Render)
            {
                Interval = new TimeSpan(0, 0, 0, 0, (int)(updateFrequency * 1000))
            };

            updateTimer.Tick += (sender, e) => Update(updateFrequency);
            updateTimer.Start();
        }

        private void Update(float updateFrequency)
        {
            // SDK callbacks can only run when you call Tick, so all of your asynchronous operations depend on it
            App.Settings.PlatformInterface?.Tick();
        }
    }
}
