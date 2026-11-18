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
    /// Standalone pop-up window presenting the progress of a single file opening request, with a cancellation
    /// cross forwarding the cancellation to the <see cref="FileOpeningController"/>. It lives outside the
    /// main window so that the progress and the cancellation remain available in tray-only or cold launch
    /// scenarios, in which the main window does not exist.
    public sealed partial class FileOpeningWindow : Window
    {
        // Effective pixels: the window carries a title or an explanation, the item card, a progress gauge with a
        // cancellation cross, and the downloaded amount, and must hug this content. Fits the worst case, i.e. a
        // two-line wrapped message.
        private const int _width = 420;
        private const int _heightWithCard = 260;
        private const int _heightCompact = 160;

        private readonly FileOpeningController _controller = App.ServiceProvider.GetRequiredService<AppModel>().FileOpeningController;
        private readonly FileOpeningRequest _request;

        public FileOpeningWindow(FileOpeningRequest request)
        {
            InitializeComponent();
            _request = request;
            RootPanel.DataContext = request;
            request.PropertyChanged += OnRequestPropertyChanged;
            CancelGaugeButton.AutomationProperties.Name = Localizer.Instance.GetString("buttonCancel");
            CancelWaitingButton.AutomationProperties.Name = Localizer.Instance.GetString("buttonCancel");
            Utility.SetWindowProperties(this, _width, ContentHeight(), Utility.WindowResizeOptions.AllowMinimize);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            AppWindow.IsShownInSwitchers = false;
            Activated += FileOpeningWindow_Activated;
        }

        /// Returns the window height for the current state of the request: compact while the item has not been
        /// identified yet (waiting for the synchronization), with room for the item card otherwise.
        private int ContentHeight() => _request.HasItemName || _request.IsDownloading ? _heightWithCard : _heightCompact;

        private void OnRequestPropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName is nameof(FileOpeningRequest.HasItemName) or nameof(FileOpeningRequest.IsDownloading))
            {
                Utility.SetWindowProperties(this, _width, ContentHeight(), Utility.WindowResizeOptions.AllowMinimize);
            }
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
