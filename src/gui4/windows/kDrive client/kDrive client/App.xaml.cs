/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

using CodeArt.MatomoTracking;
using DynamicData;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.ServerCommunication.Services;
using Infomaniak.kDrive.TrayIcon;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI.Xaml;
using Microsoft.Win32;
using Sentry;
using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive
{
    public partial class App : Application
    {
        private Window? _currentWindow;
        private UpdateWindow? _updateWindow;

        public int LegacyCommPort { get; private set; } = -1;
        public Window? CurrentWindow
        {
            get => _currentWindow;
            private set => _currentWindow = value;
        }

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
            var services = new ServiceCollection();
            services.AddSingleton<AppModel>();
            services.AddSingleton<IServerCommProtocol, SocketServerCommProtocol>();
            services.AddSingleton<IServerCommService, ServerCommService>();
            services.AddSingleton<UserDefaults>();
            services.AddSingleton<TrayIconManager>();
            services.AddSingleton<NotificationManager>();
            var configuration = new ConfigurationBuilder().Build();
            services.AddSingleton<IConfiguration>(configuration);
            services.AddMatomoTracking(options =>
            {
                options.MatomoHostname = Constants.Matomo.Host;
                options.SiteId = Constants.Matomo.SiteId;
            });
            services.AddSingleton<IAnalyticsService, MatomoService>();
            _serviceProvider = services.BuildServiceProvider();
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

            // Initialize notifications
            ServiceProvider.GetRequiredService<NotificationManager>().Init();

            ServiceProvider.GetRequiredService<TrayIconManager>().Initialize();

            var serverCommService = ServiceProvider.GetRequiredService<IServerCommService>();
            using (var cts = new CancellationTokenSource(TimeSpan.FromMinutes(5)))
            {
                if (!await serverCommService.Init(cts.Token))
                {
                    Logger.Log(Logger.Level.Fatal, "Failed to initialize server communication service, exiting application.");
                    ExitApplication();
                    return;
                }
            }

            AppModel appModel = ServiceProvider.GetRequiredService<AppModel>();
            if (!await appModel.InitializeAsync())
            {
                Logger.Log(Logger.Level.Fatal, "Application failed to initialize, exiting.");
                ExitApplication();
                return;
            }

            StartOnboardingIfNeeded();
            appModel.AllSyncs.AsObservableChangeSet()
            .Subscribe(_ =>
            {
                StartOnboardingIfNeeded();
            });
        }

        public enum CreateWindowOptions
        {
            Foreground = 1,
            CancelOnboarding = 2,
            OpenSettings = 4
        }
        public void CreateWindow(CreateWindowOptions options)
        {
            if (CurrentWindow is OnBoardingWindow && options.HasFlag(CreateWindowOptions.CancelOnboarding))
            {
                CurrentWindow.Close();
                CurrentWindow = null;
            }

            if (CurrentWindow is null)
            {
                var appModel = ServiceProvider.GetRequiredService<AppModel>();
                if (options.HasFlag(CreateWindowOptions.CancelOnboarding) || !StartOnboardingIfNeeded())
                {
                    CurrentWindow = new MainWindow(options.HasFlag(CreateWindowOptions.OpenSettings) ? typeof(Pages.Settings.SettingsPage) : null);
                }
                else
                {
                    options &= ~CreateWindowOptions.Foreground; // StartOnboarding will handle bringing the window to the front, so we can skip it here to avoid unnecessary calls.
                }
            }
            else if (CurrentWindow is MainWindow mainWindow && options.HasFlag(CreateWindowOptions.OpenSettings))
            {
                mainWindow?.AppNavView?.Frame?.Navigate(typeof(Pages.Settings.SettingsPage));
            }

            if (options.HasFlag(CreateWindowOptions.Foreground))
                Utility.BringCurrentWindowToFront();
        }

        async void OnProcessExit(object? sender, EventArgs e)
        {
            await ServiceProvider.GetRequiredService<NotificationManager>().UnregisterAsync();
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

                var previousWindow = CurrentWindow;
                CurrentWindow = new OnBoarding.OnBoardingWindow();
                previousWindow?.Close();

                ((OnBoarding.OnBoardingWindow)CurrentWindow).Closed += OnOnboardingClosed;
                Utility.BringCurrentWindowToFront();
            });
        }

        private void OnOnboardingClosed(object sender, WindowEventArgs e)
        {
            Logger.Log(Logger.Level.Info, "OnBoardingWindow closed, restarting MainWindow.");

            var onboardingWindow = (OnBoarding.OnBoardingWindow)sender;
            onboardingWindow.Closed -= OnOnboardingClosed;
            CurrentWindow = null;
            CreateWindow(CreateWindowOptions.CancelOnboarding | CreateWindowOptions.Foreground);
        }

        public bool StartOnboardingIfNeeded()
        {
            var appModel = ServiceProvider.GetRequiredService<AppModel>();

            if (appModel.IsInitialized && !appModel.AllSyncs.Any() && !(CurrentWindow is OnBoarding.OnBoardingWindow))
            {
                Logger.Log(Logger.Level.Info, "No users available after initialization, starting onboarding process.");
                StartOnboarding();
                return true;
            }
            return false;
        }

        public static void RestartApplicationWindows()
        {
            Logger.Log(Logger.Level.Info, $"Restarting all application windows.");
            App app = (App)Current;
            app.CloseUpdateWindow();
            var previousWindow = app.CurrentWindow;
            app.CurrentWindow = null;
            previousWindow?.Close();
            app.CreateWindow(CreateWindowOptions.Foreground);
        }
        public static void ExitApplication()
        {
            SentrySdk.Flush(new TimeSpan(0, 0, 5));
            Logger.Log(Logger.Level.Info, "Exiting application.");
            Environment.Exit(0);
        }

        public static void ExitApplicationAndShutdownServer()
        {
            Logger.Log(Logger.Level.Info, "Sending exit command to server.");
            App.ServiceProvider.GetRequiredService<IServerCommService>().Exit();
            ExitApplication();
        }

        public void ShowUpdateWindow()
        {

            AppModel.UIThreadDispatcher.TryEnqueue(async () =>
            {
                const int maxRetries = 10;
                int retryCount = 0;

                while (ServiceProvider.GetRequiredService<AppModel>().Settings.UpdateManager.AvailableUpdate is null &&
                           retryCount < maxRetries)
                {
                    Logger.Log(Logger.Level.Info,
                                   $"ShowUpdateWindow called but no available update found, retrying in 1 seconds ({retryCount + 1}/{maxRetries}).");

                    retryCount++;
                    await Task.Delay(TimeSpan.FromSeconds(1));
                }

                if (ServiceProvider.GetRequiredService<AppModel>().Settings.UpdateManager.AvailableUpdate is null)
                {
                    Logger.Log(Logger.Level.Warning,
                                   "ShowUpdateWindow aborted after retries because no available update was found.");
                    return;
                }

                if (_updateWindow is null)
                {
                    _updateWindow = new UpdateWindow();
                    _updateWindow.Closed += (s, e) => _updateWindow = null;
                    _updateWindow.Activate();
                }
                else
                {
                    Logger.Log(Logger.Level.Info,
                                   "Update window is already open, bringing existing window to front.");
                }
                Utility.BringWindowToFront(_updateWindow);
            });

        }

        public void CloseUpdateWindow()
        {
            AppModel.UIThreadDispatcher.TryEnqueue(() =>
            {
                if (_updateWindow is not null)
                {
                    _updateWindow.Close();
                    _updateWindow = null;
                }
            });
        }
    }
}
