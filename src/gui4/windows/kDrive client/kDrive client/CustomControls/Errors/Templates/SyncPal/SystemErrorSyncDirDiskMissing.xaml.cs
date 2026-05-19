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
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.SyncDirDiskMissing },
        NodeTypes = new[] { NodeType.File, NodeType.Directory, NodeType.Unknown },
        ShowInSystemTray = true
    )]
    public sealed partial class SystemErrorSyncDirDiskMissing : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private Error Error { get; init; }
        public SystemErrorSyncDirDiskMissing(Error error)
        {
            this.InitializeComponent();
            Error = error;

            error.Path = Error.Sync?.LocalPath ?? string.Empty;
            error.NodeType = Types.NodeType.Directory;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            AutoDisposeDialog dialog = new AutoDisposeDialog
            {
                XamlRoot = xamlRoot,
                Title = Localizer.Instance.GetString("errDialogSystemSyncDirDiskMissingTitle"),
                DefaultButton = ContentDialogButton.Primary,
                CloseButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonRestartSync"),
                Content = new SystemErrorSyncDirDiskMissingErrorDialog(Error) { XamlRoot = xamlRoot }
            };
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageSyncDirDiskMissing);
            var res = await dialog.ShowAsync();
            if (res == ContentDialogResult.Primary)
            {
                if (Error.Sync is null)
                {
                    Logger.Log(Logger.Level.Error, "Error.Sync is null in SystemErrorSyncDirDiskMissing. Cannot proceed with action click.");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }
                _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.SyncDirDiskMissingRestartSync);
                if (!await Error.Sync.Start())
                {
                    Logger.Log(Logger.Level.Error, "Failed to restart sync in SystemErrorSyncDirDiskMissing.");
                    Utility.ShowTeachingTip(Localizer.Instance.GetString("errDialogSystemSyncDirDiskMissingTitle"));
                }
            }
        }
    }
}