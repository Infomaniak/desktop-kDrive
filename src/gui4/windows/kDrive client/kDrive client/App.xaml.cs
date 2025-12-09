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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Security.Authentication.OAuth;
using Microsoft.UI.Xaml;
using Microsoft.Win32;
using System;
using System.Diagnostics;
using System.Linq;


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
                TrayIcoManager.ConfigureWindowEventHandler();
            }
        }
        public TrayIcon.TrayIconManager TrayIcoManager { get; private set; }

        private readonly IServiceCollection _services = new ServiceCollection();
        private static IServiceProvider? _serviceProvider = null;
        internal static IServiceProvider ServiceProvider => _serviceProvider ?? throw new InvalidOperationException("Service provider is not initialized.");

        internal static IAppConstants Constants => new ProductionConstants();
        internal App()
        {
            InitializeComponent();
            TrayIcoManager = new TrayIcon.TrayIconManager();
            _services.AddSingleton<AppModel>();
            _services.AddSingleton<IServerCommProtocol, SocketServerCommProtocol>();
            //_services.AddSingleton<IServerCommProtocol, MockServerCommProtocol>();
            _services.AddSingleton<IServerCommService, ServerCommService>();
            _serviceProvider = _services.BuildServiceProvider();
            Logger.Log(Logger.Level.Info, "Application started");
        }

        protected override async void OnLaunched(Microsoft.UI.Xaml.LaunchActivatedEventArgs args)
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
                LegacyCommPort = Int32.Parse(arguments[1]);
            }
            // Register oAuth protocol handler
            RegisterOAuthProtocol();

            // Start all singleton services
            foreach (var serviceDescriptor in _services.Where(sd => sd.Lifetime == ServiceLifetime.Singleton))
            {
                // Force the initialization of singleton services
                ServiceProvider.GetRequiredService(serviceDescriptor.ServiceType);
            }

            CurrentWindow = new MainWindow();
            TrayIcoManager.Initialize();
            AppModel appModel = ServiceProvider.GetRequiredService<AppModel>();
            await appModel.InitializeAsync();
            StartOnboardingIfNeeded();
            appModel.AllSyncs.AsObservableChangeSet()
            .Subscribe(_ =>
            {
                StartOnboardingIfNeeded();
            });

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
                ((OnBoarding.OnBoardingWindow)CurrentWindow).Closed += (s, e) =>
                {
                    if (ServiceProvider.GetRequiredService<AppModel>().Users.Any())
                    {
                        Logger.Log(Logger.Level.Info, "OnBoardingWindow closed, restarting MainWindow.");
                        CurrentWindow = new MainWindow();
                        CurrentWindow.Activate();
                    }
                };
                CurrentWindow.Activate();
            });
        }

        public void StartOnboardingIfNeeded()
        {
            if (!ServiceProvider.GetRequiredService<AppModel>().Users.Any() && !(CurrentWindow is OnBoarding.OnBoardingWindow))
            {
                Logger.Log(Logger.Level.Info, "No users available after initialization, starting onboarding process.");
                StartOnboarding();
            }
        }
    }
}
