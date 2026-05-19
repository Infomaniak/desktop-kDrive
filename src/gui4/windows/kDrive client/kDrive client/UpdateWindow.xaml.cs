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
using System;

namespace Infomaniak.kDrive
{
    public sealed partial class UpdateWindow : Window
    {
        private const int _minimumWidth = 540;
        private const int _minimumHeight = 467;
        public AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public UpdateWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
                                                     // this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, _minimumWidth, _minimumHeight, Utility.WindowResizeOptions.None); // Set initial size and prevent resizing
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            Activated += UpdateWindow_Activated;
            Closed += UpdateWindow_Closed;
        }

        private async void UpdateWindow_Closed(object sender, WindowEventArgs args)
        {
            await Utility.VisualTreeDisposeUtility.DisposeItemsAsync(this.Content);
        }

        private void UpdateWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            Utility.CenterWindow(this);
        }
    }
}
