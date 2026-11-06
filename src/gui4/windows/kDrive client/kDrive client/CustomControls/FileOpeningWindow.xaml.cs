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

namespace Infomaniak.kDrive.CustomControls
{
    /// Standalone pop-up window presenting the progress of a single file opening request, with a Cancel
    /// button forwarding the cancellation to the <see cref="FileOpeningController"/>. It lives outside the
    /// main window so that the progress and the cancellation remain available in tray-only or cold launch
    /// scenarios, in which the main window does not exist.
    public sealed partial class FileOpeningWindow : Window
    {
        // Effective pixels: the window only carries an item name, a short message, a progress gauge and a Cancel
        // button, and must hug this content. Fits the worst case, i.e. a two-line wrapped message.
        private const int _width = 420;
        private const int _height = 220;

        private readonly FileOpeningController _controller = App.ServiceProvider.GetRequiredService<AppModel>().FileOpeningController;

        public FileOpeningWindow(FileOpeningRequest request)
        {
            InitializeComponent();
            RootPanel.DataContext = request;
            CancelButton.Content = Localizer.Instance.GetString("buttonCancel");
            Utility.SetWindowProperties(this, _width, _height, Utility.WindowResizeOptions.AllowMinimize);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            AppWindow.IsShownInSwitchers = false;
            Activated += FileOpeningWindow_Activated;
        }

        private void FileOpeningWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            Utility.CenterWindow(this);
            Activated -= FileOpeningWindow_Activated;
        }

        private async void OnCancelClicked(object sender, RoutedEventArgs e)
        {
            if (RootPanel.DataContext is not FileOpeningRequest request)
                return;

            // The server confirms the cancellation by reporting the Finished phase of the request, which closes
            // this window. Until then, the window stays open and the progress keeps being refreshed.
            await _controller.CancelRequestAsync(request.JobId);
        }
    }
}
