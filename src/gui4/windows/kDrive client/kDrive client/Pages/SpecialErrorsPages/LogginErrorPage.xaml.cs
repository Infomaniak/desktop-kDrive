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

using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class LogginErrorPage : SpecialErroBasePage
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        public struct NavigationParams
        {
            public bool TryReconnectOnLoad { get; set; }
        }
        private NavigationParams? _navigationParams;
        public LogginErrorPage() : base([SyncErrorStates.LoggingError])
        {
            Logger.Log(Logger.Level.Info, "Navigated to LogginErrorPage - Initializing LogginErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "LogginErrorPage components initialized");
            Loaded += OnLoaded;
        }

        private async void OnLoaded(object sender, RoutedEventArgs e)
        {
            if(_navigationParams?.TryReconnectOnLoad == true)
            {
                DispatcherQueue.TryEnqueue(async () => await StartConnection());
            }
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);
            _navigationParams = e.Parameter as NavigationParams?;
            _analyticsService.TrackPageView(Analytics.Keys.Category.LogginErrorPage);
        }

        private async void ConnectionButton_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.LogginErrorPage, Analytics.Keys.EventName.OpenSignInWeb);
            await StartConnection();
        }

        private async Task StartConnection()
        {
            try
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(120));
                var OAutCodes = await OAuthHelper.GetCode(cts.Token);
                if (OAutCodes.Code == "")
                {
                    Logger.Log(Logger.Level.Warning, "Failed to obtain user code.");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                Logger.Log(Logger.Level.Debug, "Successfully obtained user code.");
                User? user = await App.ServiceProvider.GetRequiredService<IServerCommService>().AddOrRelogUser(OAutCodes.Code, OAutCodes.CodeVerifier, CancellationToken.None);
                if (user is null || ViewModel.SelectedSync is null)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to retrieve user information after authentication {user} - {ViewModel.SelectedSync}");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                if (user.DbId != ViewModel.SelectedSync.Drive.Account.User.DbId)
                {
                    Logger.Log(Logger.Level.Info, "Authenticated user does not match the expected user.");
                    DisplayUserMismatchContent();
                    return;
                }

                if (await ViewModel.SelectedSync.Start())
                    return;

                Logger.Log(Logger.Level.Error, "Failed to start sync.");
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Warning, "Authentication process canceled by user.");
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Authentication process failed {ex.Message}");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }

        private void DisplayUserMismatchContent()
        {
            TitleTextBlock.Text = Localizer.Instance.GetString("driveLoggingErrorUserMismatchTitle");
            SubtitleTextBlock.Text = Localizer.Instance.GetString("driveLoggingErrorUserMismatchDescription", Utility.ObfuscateEmail(ViewModel.SelectedSync?.Drive.Account.User.Email));
        }
    }
}
