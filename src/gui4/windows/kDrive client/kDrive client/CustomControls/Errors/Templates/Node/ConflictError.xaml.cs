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
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Window;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ConflictTypes = new[] { ConflictType.CreateCreate, ConflictType.EditEdit },
        ShowInSystemTray = true
    )]
    public sealed partial class ConflictError : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        private Error Error { get; init; }
        public ConflictError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                DefaultButton = ContentDialogButton.Secondary,
                PrimaryButtonText = Localizer.Instance.GetString("buttonClose"),
            };

            // Set the content first to allow the dialog to measure properly
            dialog.Content = new ConflictDialog(Error, dialog) { XamlRoot = xamlRoot };

            // Apply the style to allow wider content
            dialog.Resources["ContentDialogMaxWidth"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxWidth"];
            dialog.Resources["ContentDialogMaxHeight"] = Application.Current.Resources["Infomaniak.Style.ContentDialog.MaxHeight"];
            _analyticsService.TrackClick(Analytics.Keys.Category.Errors, Analytics.Keys.EventName.ManageSingleConflict);
            _ = await dialog.ShowAsync();
            await Utility.VisualTreeDisposeUtility.DisposeItemsAsync(dialog);
        }
    }
}