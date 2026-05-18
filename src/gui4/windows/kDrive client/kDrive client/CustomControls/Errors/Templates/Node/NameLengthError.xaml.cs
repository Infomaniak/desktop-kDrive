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

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.NameLength }
    )]
    public sealed partial class NameLengthError : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private Error Error { get; init; }
        public NameLengthError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {

            if (Error.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Error.Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageFileNameTooLong);


            if (!string.IsNullOrEmpty(Error.RemoteNodeId) && !await Error.OpenItemInWebViewAsync())
                Utility.ShowUnexpectedErrorTeachingTip();
            else if (!string.IsNullOrEmpty(Error.Path) && !await Utility.OpenFolderSecurely(System.IO.Path.Combine(Error.Sync.LocalPath, Error.Path)))
                Utility.ShowUnexpectedErrorTeachingTip();
            else if (string.IsNullOrEmpty(Error.RemoteNodeId) && string.IsNullOrEmpty(Error.Path))
            {
                Logger.Log(Logger.Level.Warning, $"Error {Error.DbId} has no RemoteNodeId or Path, cannot open in web or file explorer.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

        }
    }
}