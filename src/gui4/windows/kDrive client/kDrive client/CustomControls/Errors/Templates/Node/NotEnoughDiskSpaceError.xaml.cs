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
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.NotEnoughDiskSpace },
        ShowInSystemTray = true
    )]
    public sealed partial class NotEnoughDiskSpaceError : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private Error Error { get; init; }
        public NotEnoughDiskSpaceError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            if (Error.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Error.Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageNotEnoughDiskSpace);

            if (Error.Sync.LocalPath.StartsWith("C:"))
            {
                bool result = await Windows.System.Launcher.LaunchUriAsync(new Uri("ms-settings:storagesense"));
                if (!result)
                {
                    Logger.Log(Logger.Level.Warning, "Failed to launch settings for NotEnoughDiskSpaceError");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
            }
            else
            {
                // Windows only provide a storage manager for the main disk, else we just open the disk at root so the user can manually clean it.
                var path = System.IO.Path.GetPathRoot(Error.Sync.LocalPath);
                if (path is null || !await Utility.OpenFolderSecurely(path))
                {
                    Logger.Log(Logger.Level.Warning, $"Failed to open sync root path {Error.Sync.LocalPath}");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
            }
        }
    }
}