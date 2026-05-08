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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.InvalidSync },
        ExitCauses = new[] { ExitCause.SyncDirAccessError },
        ShowInSystemTray = true
    )]
    public sealed partial class InvalidSyncSyncDirAccessError : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private Error Error { get; init; }
        public InvalidSyncSyncDirAccessError(Error error)
        {
            this.InitializeComponent();
            Error = error;
            error.Path = Error.Sync?.RemotePath ?? string.Empty;
            error.NodeType = Types.NodeType.Directory;
        }
        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                Title = Localizer.Instance.GetString("errDialogConfigChangedTitle"),
                DefaultButton = ContentDialogButton.Primary,
                CloseButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonCreateNewSync"),
                Content = new InvalidSyncSyncDirAccessErrorDialog(Error) { XamlRoot = xamlRoot }
            };
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageInvalidRemoteSyncDir);

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                var frame = ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;
                if (frame is null)
                {
                    Logger.Log(Logger.Level.Error, "Failed to navigate to the sync setup page after a sync directory change error because the main frame could not be found.");
                    return;
                }

                if (Error.Sync is null)
                {
                    Logger.Log(Logger.Level.Error, "Error.Sync is null");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                var destPage = Error.Sync.IsAdvanced ? typeof(Pages.Settings.DriveAdvancedSyncsPage) : typeof(Pages.Settings.DriveManagementPage);
                _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.SyncDirAccessErrorOpenFolder);
                frame.Navigate(destPage, Error.Sync.Drive);
            }
        }
    }
}
