/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using DynamicData;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.TrayIcon;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI.Xaml;
using Microsoft.Win32;
using Microsoft.Windows.AppLifecycle;
using Sentry;
using System;
using System.Diagnostics;
using System.Linq;
using Windows.ApplicationModel.Core;
using Windows.Foundation;
using Microsoft.Windows.AppNotifications;

namespace Infomaniak.kDrive
{
    public partial class App : Application
    {
        private Window? _currentWindow;

        public int LegacyCommPort { get; private set; } = -1;
        public Window? CurrentWindow
        {
            get => _currentWindow;
            private set
            {
                _currentWindow = value;
                ServiceProvider.GetRequiredService<TrayIconManager>().ConfigureWindowEventHandler();
            }
        }

        private readonly IServiceCollection _services = new ServiceCollection();
        private static IServiceProvider? _serviceProvider = null;
        internal static IServiceProvider ServiceProvider => _serviceProvider ?? throw new InvalidOperationException("Service provider is not initialized.");

        /* 
         * Application-wide constants instance.
         * 
         * By default, the app uses ProductionAppConstants. For testing purposes, you can switch to a custom instance 
         * with mock or staging values by returning a CustomAppConstants instead.
         * 
         * Example for testing:
         * internal static IAppConstants Constants => new CustomAppConstants(
         *     new ProductionSentry(),
         *     new ProductionGitHub(),
         *     new ProductionDrive(),
         *     new ProductionStorage(),
         *     new PreProdLogin(),
         *     new ProductionKSuite());
         */

        internal static IAppConstants Constants => new ProductionAppConstants();


        internal App()
        {
            _services.AddSingleton<AppModel>();
            _services.AddSingleton<IServerCommProtocol, SocketServerCommProtocol>();
            _services.AddSingleton<IServerCommService, ServerCommService>();
            _services.AddSingleton<UserDefaults>();
            _services.AddSingleton<TrayIconManager>();
            _services.AddSingleton<NotificationManager>();
            _serviceProvider = _services.BuildServiceProvider();
            AppDomain.CurrentDomain.ProcessExit += new EventHandler(OnProcessExit);

            Logger.StartSentry();
            InitializeComponent();
            Logger.Log(Logger.Level.Info, "Application started");
        }

        protected override async void OnLaunched(LaunchActivatedEventArgs args)
        {
            string[] arguments = Environment.GetCommandLineArgs();
            if (arguments.Length > 1)
            {
                var oauthArg = arguments.FirstOrDefault(arg => arg.StartsWith("kdrive://auth-desktop"), "");
                if (oauthArg != "")
                {
                    if (OAuth2Manager.CompleteAuthRequest(new Uri(oauthArg)))
                    {
                        // Terminate the Process
                        Logger.Log(Logger.Level.Info, "OAuth process completed, response routed successfully. Terminating the process.");
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Warning, "OAuth process failed.");
                    }
                    Process current = Process.GetCurrentProcess();
                    current.Kill();
                    return;
                }
                try
                {
                    LegacyCommPort = Int32.Parse(arguments[1]);
                    Logger.Log(Logger.Level.Info, $"Parsed legacy communication port from arguments: {LegacyCommPort}");
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to parse legacy communication port from arguments {ex}");
                }
            }
            // Register oAuth protocol handler
            RegisterOAuthProtocol();

            CurrentWindow = new MainWindow();
            var currentWindowContent = CurrentWindow.Content;

            // Initialize notifications
            ServiceProvider.GetRequiredService<NotificationManager>().Init();

            // Display splash screen
            CurrentWindow.Content = new CustomControls.SplashScreen();
            ServiceProvider.GetRequiredService<TrayIconManager>().Initialize();

            AppModel appModel = ServiceProvider.GetRequiredService<AppModel>();


            // Start all singleton services
            foreach (var serviceDescriptor in _services.Where(sd => sd.Lifetime == ServiceLifetime.Singleton))
            {
                // Force the initialization of singleton services
                ServiceProvider.GetRequiredService(serviceDescriptor.ServiceType);
            }
            ServiceProvider.GetRequiredService<IServerCommProtocol>().ConnectionLost += (s, e) =>
            {
                Logger.Log(Logger.Level.Fatal, "Connection to server lost, attempting to restart application.");
                SentrySdk.Flush(new TimeSpan(0, 0, 5));
                AppRestartFailureReason restartError = AppInstance.Restart(LegacyCommPort.ToString());
                if (restartError != AppRestartFailureReason.Other)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to restart application after connection lost: {restartError}");
                }
            };

            if (!await appModel.InitializeAsync())
            {
                Logger.Log(Logger.Level.Fatal, "Application failed to initialize, exiting.");
                ExitApplication();
                return;
            }
            CurrentWindow.Content = currentWindowContent;
            StartOnboardingIfNeeded();
            appModel.AllSyncs.AsObservableChangeSet()
            .Subscribe(_ =>
            {
                StartOnboardingIfNeeded();
            });
        }

        void OnProcessExit(object sender, EventArgs e)
        {
            ServiceProvider.GetRequiredService<NotificationManager>().Unregister();
        }

        private void RegisterOAuthProtocol()
        {
            const string protocol = "kDrive";
            string exe = Environment.ProcessPath ?? "";
            if (exe == "")
            {
                Logger.Log(Logger.Level.Error, "Failed to register oauth protocol handler: unable to determine executable path.");
                return;
            }

            using var key = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{protocol}");
            key.SetValue("", $"URL:{protocol} protocol");
            key.SetValue("URL Protocol", "");

            using var icon = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{protocol}\DefaultIcon");
            icon.SetValue("", $"{exe},1");

            using var command = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{protocol}\shell\open\command");
            command.SetValue("", $"\"{exe}\" \"%1\"");
        }


        public void StartOnboarding()
        {
            AppModel.UIThreadDispatcher.TryEnqueue(() =>
            {
                if (CurrentWindow?.GetType() == typeof(OnBoarding.OnBoardingWindow))
                {
                    Logger.Log(Logger.Level.Info, "OnBoardingWindow is already open, skipping StartOnboarding call.");
                    return;
                }
                CurrentWindow?.Close();
                CurrentWindow = new OnBoarding.OnBoardingWindow();
                TypedEventHandler<object, WindowEventArgs> closedEventHandler = (s, e) =>
                {
                    if (ServiceProvider.GetRequiredService<AppModel>().Users.Any())
                    {
                        Logger.Log(Logger.Level.Info, "OnBoardingWindow closed, restarting MainWindow.");
                        // Detach the event handler to avoid multiple calls
                        ((OnBoarding.OnBoardingWindow)CurrentWindow).Closed += OnOnboardingClosed;

                        CurrentWindow = new MainWindow();
                        CurrentWindow.Activate();
                    }
                };

                ((OnBoarding.OnBoardingWindow)CurrentWindow).Closed += closedEventHandler;
                CurrentWindow.Activate();
            });
        }

        private void OnOnboardingClosed(object sender, WindowEventArgs e)
        {
            if (!ServiceProvider.GetRequiredService<AppModel>().Users.Any())
                return;

            Logger.Log(Logger.Level.Info, "OnBoardingWindow closed, restarting MainWindow.");

            var onboardingWindow = (OnBoarding.OnBoardingWindow)sender;
            onboardingWindow.Closed -= OnOnboardingClosed;

            CurrentWindow = new MainWindow();
            CurrentWindow.Activate();
        }

        public void StartOnboardingIfNeeded()
        {
            if (!ServiceProvider.GetRequiredService<AppModel>().Users.Any() && !(CurrentWindow is OnBoarding.OnBoardingWindow))
            {
                Logger.Log(Logger.Level.Info, "No users available after initialization, starting onboarding process.");
                StartOnboarding();
            }
        }

        public static void ExitApplication()
        {
            Logger.Log(Logger.Level.Info, "Exiting application.");
            (Current as App)!.CurrentWindow?.Close();
            Environment.Exit(0);
        }

        public static void ExitApplicationAndShutdownServer()
        {
            Logger.Log(Logger.Level.Info, "Sending exit command to server.");
            App.ServiceProvider.GetRequiredService<IServerCommService>().Exit();
            ExitApplication();
        }
    }
}
