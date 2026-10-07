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
using Microsoft.UI.Xaml.Controls;
using System;
using System.Threading;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.SystemError},
        ExitCauses = new[] { ExitCause.HardlinkNotSupported }
    )]
    public sealed partial class HardlinkError : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        private Error Error { get; init; }
        public HardlinkError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            if (Error.Sync is null)
            {
                Logger.LogError("Error.Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (string.IsNullOrEmpty(Error.LocalNodeId))
            {
                Logger.LogError("Error.LocalNodeId is null or empty. Cannot quickly resolve the hardlink error.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageHardlinkError);

            try
            {
                var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
                bool success = await commService.QuickResolveHardlink(Error.Sync.DbId, Error.DbId, Error.LocalNodeId, Error.Path, CancellationToken.None);
                if (!success)
                {
                    Logger.LogError($"Failed to quickly resolve the hardlink error with DbId {Error.DbId}",
                        "HardlinkError: Failed to quickly resolve the hardlink error");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                // On success, the server sends an ErrorRemoved signal that removes the error card from the UI.
            }
            catch (Exception ex)
            {
                Logger.LogError($"Failed to quickly resolve the hardlink error with DbId {Error.DbId}. Exception: {ex.Message}",
                    "HardlinkError: Failed to quickly resolve the hardlink error");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }
    }
}