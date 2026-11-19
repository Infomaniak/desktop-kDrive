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
using Microsoft.UI.Xaml;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    /// Standalone pop-up window informing the user that a file opening requested through a "kdrive://open/..." deeplink
    /// failed, and offering to download the item from the kDrive web application in a browser. It replaces the
    /// progress pop-up window of the request when the server reports the Failed phase, and stays open until the user
    /// closes it.
    public sealed partial class FileOpeningFailureWindow : Window
    {
        // Effective pixels: the window carries a failure title, an explanation that wraps on two lines, the item
        // card, and the two actions, and must hug this content.
        private const int _width = 420;
        private const int _height = 300;

        private readonly FileOpeningRequest _request;

        public FileOpeningFailureWindow(FileOpeningRequest request)
        {
            InitializeComponent();
            _request = request;
            RootPanel.DataContext = request;
            FailureTitleTextBlock.Text = Localizer.Instance.GetString("fileOpeningFailureTitle");
            FailureExplanationTextBlock.Text = Localizer.Instance.GetString("fileOpeningFailureExplanation");
            CloseButton.Content = Localizer.Instance.GetString("buttonCancel");
            DownloadButton.Content = Localizer.Instance.GetString("fileOpeningDownloadButton");
            Utility.SetWindowProperties(this, _width, _height, Utility.WindowResizeOptions.None);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            AppWindow.IsShownInSwitchers = false;
            Activated += FileOpeningFailureWindow_Activated;
        }

        private void FileOpeningFailureWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            Utility.CenterWindow(this);
            Activated -= FileOpeningFailureWindow_Activated;
        }

        private async void OnDownloadClicked(object sender, RoutedEventArgs e)
        {
            // Open the kDrive web application page of the item in a browser, from which the user can download it
            // manually.
            if (!Uri.TryCreate(_request.WebUrl, UriKind.Absolute, out var uri))
            {
                Logger.LogError($"Invalid file opening web page url: {_request.WebUrl}", "FileOpeningFailureWindow: Invalid web page url");
                return;
            }

            await Windows.System.Launcher.LaunchUriAsync(uri);
        }

        private void OnCloseClicked(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
