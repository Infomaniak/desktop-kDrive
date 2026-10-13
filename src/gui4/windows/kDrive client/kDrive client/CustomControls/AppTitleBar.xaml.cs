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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class AppTitleBar : TitleBar
    {
        public AppModel ViewModel { get; } =
           App.ServiceProvider.GetRequiredService<AppModel>();

        private Frame? Frame => ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView?.Frame ?? null;

        public AppTitleBar()
        {
            InitializeComponent();
        }

        private Visibility TitleBarContentVisibility(bool isModelInitialized, Sync? selectedSync, bool isUpdateRequired)
        {
            if (!isModelInitialized || isUpdateRequired)
                return Visibility.Collapsed;
            if (selectedSync is null)
                return Visibility.Collapsed;

            return Visibility.Visible;
        }

        private void TitleBar_BackRequested(TitleBar sender, object args)
        {
            if (Frame is not null && Frame.CanGoBack)
                Frame?.GoBack();
        }
    }
}
