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

using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Text.RegularExpressions;
using System.Threading;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class LoggingErrorPage : SpecialErroBasePage
    {
        public LoggingErrorPage() : base([SyncErrorStates.LoggingError])
        {
            Logger.Log(Logger.Level.Info, "Navigated to LoggingErrorPage - Initializing LoggingErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "LoggingErrorPage components initialized");
        }

        private async void ConnectionButton_Click(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
            {
                control.Visibility = Visibility.Collapsed;
                OAuthProgressRing.Visibility = Visibility.Visible;
            }

            try
            {
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(120));
                var OAutCodes = await OAuthHelper.GetCode(cts.Token);
                if (OAutCodes.Code != "")
                {
                    Logger.Log(Logger.Level.Debug, "Successfully obtained user code.");
                    User? user = await App.ServiceProvider.GetRequiredService<IServerCommService>().AddOrRelogUser(OAutCodes.Code, OAutCodes.CodeVerifier, CancellationToken.None);
                    if (user is not null && user.DbId == ViewModel.SelectedSync?.Drive.Account.User.DbId)
                    {
                        ViewModel.SelectedSync?.Start();
                    }
                    else if (user is not null)
                    {
                        DisplayUserMismatchContent();
                    }
                    else
                    {
                        Utility.ShowUnexpectedErrorTeachingTip();
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "Authentication process failed");
                }
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
            finally
            {
                if (control is not null)
                {
                    control.Visibility = Visibility.Visible;
                    OAuthProgressRing.Visibility = Visibility.Collapsed;
                }
            }
        }

        private void DisplayUserMismatchContent()
        {
            TitleTextBlock.Text = Utility.GetLocalizedString("Page_LogginErrorPage_UserMissmatch_Title/Text");
            SubtitleTextBlock.Text = Utility.GetLocalizedString("Page_LogginErrorPage_UserMissmatch_Subtitle/Text", Utility.ObfuscateEmail(ViewModel.SelectedSync?.Drive.Account.User.Email));
            ConnectionButton.Content = Utility.GetLocalizedString("Page_LogginErrorPage_UserMissmatch_Button/Content");
        }
    }
}
