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
 */using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    /// Standalone pop-up window informing the user that a file opening requested through a "kdrive://open/..." deeplink
    /// failed, and offering to download the item into the download folder of the user. It replaces the progress
    /// pop-up window of the request when the server reports a failure phase. When the item does not exist on the
    /// remote side (NotFound), there is nothing to download: the window shows a dedicated message and offers no
    /// download. While the download triggered from this window runs, it shows its progress; when it fails, the
    /// window shows an error and the download button downloads the item again.
    public sealed partial class FileOpeningFailureWindow : Window
    {
        // Effective pixels: the window carries a failure title, an explanation that wraps on two lines, the item
        // card, and the two actions, and must hug this content. The not-found variant drops the explanation and the
        // download button, and the failed download adds the error message.
        private const int _width = 420;
        private const int _height = 300;
        private const int _heightNotFound = 180;
        private const int _heightDownloadFailed = 336;

        private readonly FileOpeningRequest _request;

        public FileOpeningFailureWindow(FileOpeningRequest request)
        {
            InitializeComponent();
            _request = request;
            RootPanel.DataContext = request;
            _request.PropertyChanged += Request_PropertyChanged;
            if (request.IsNotFound)
            {
                FailureTitleTextBlock.Text = Localizer.Instance.GetString("fileOpeningNotFoundMessage");
                FailureExplanationTextBlock.Visibility = Visibility.Collapsed;
                DownloadButton.Visibility = Visibility.Collapsed;
            }
            else
            {
                FailureTitleTextBlock.Text = Localizer.Instance.GetString("fileOpeningFailureTitle");
                FailureExplanationTextBlock.Text = Localizer.Instance.GetString("fileOpeningFailureExplanation");
                DownloadButton.Content = Localizer.Instance.GetString("fileOpeningDownloadButton");
                // The download of the item into the download folder of the user requires its identity, reported by
                // the failure signal: hide the button when it is unknown.
                DownloadButton.Visibility = request.HasItemIdentity ? Visibility.Visible : Visibility.Collapsed;
            }

            CloseButton.Content = Localizer.Instance.GetString("buttonCancel");
            Utility.SetWindowProperties(this, _width, ContentHeight(), Utility.WindowResizeOptions.None);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            AppWindow.IsShownInSwitchers = false;
            Activated += FileOpeningFailureWindow_Activated;
        }

        /// Content height of the window for the current state of the request: the download of the item into the
        /// download folder of the user replaces the download button with its progress, and its failure adds an error
        /// message to the explanation.
        private int ContentHeight()
        {
            if (_request.IsNotFound) return _heightNotFound;
            return _request.IsItemDownloadFailed ? _heightDownloadFailed : _height;
        }

        private void FileOpeningFailureWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            Utility.CenterWindow(this);
            Activated -= FileOpeningFailureWindow_Activated;
        }

        private void Request_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            switch (e.PropertyName)
            {
                case nameof(FileOpeningRequest.IsItemDownloading):
                case nameof(FileOpeningRequest.IsItemDownloadFailed):
                    // The download of the item into the download folder of the user started or reached its terminal
                    // phase: swap the download button for its progress, or add the error message, and resize.
                    ItemDownloadProgressPanel.Visibility = _request.IsItemDownloading ? Visibility.Visible : Visibility.Collapsed;
                    ItemDownloadErrorTextBlock.Text = _request.IsItemDownloadFailed
                        ? Localizer.Instance.GetString("fileOpeningDownloadFailureMessage") : "";
                    ItemDownloadErrorTextBlock.Visibility = _request.IsItemDownloadFailed ? Visibility.Visible : Visibility.Collapsed;
                    UpdateItemDownloadProgress();
                    Utility.SetWindowProperties(this, _width, ContentHeight(), Utility.WindowResizeOptions.None);
                    break;
                case nameof(FileOpeningRequest.HasItemDownloadProgress):
                    UpdateItemDownloadProgress();
                    break;
            }
        }

        private void UpdateItemDownloadProgress()
        {
            ItemDownloadProgressBar.Maximum = _request.ItemDownloadGaugeMaximum;
            ItemDownloadProgressBar.Value = _request.ItemDownloadGaugeValue;
            ItemDownloadProgressBar.IsIndeterminate = _request.IsItemDownloading && !_request.HasItemDownloadProgress;
            ItemDownloadProgressTextBlock.Text = _request.HasItemDownloadProgress ? _request.ItemDownloadProgressText : "";
        }

        private async void OnDownloadClicked(object sender, RoutedEventArgs e)
        {
            // Download the item into the download folder of the user: the window shows the progress of the download,
            // and closes once the item has been downloaded. On a failure, the button downloads the item again.
            var fileOpeningController = App.ServiceProvider.GetRequiredService<AppModel>().FileOpeningController;
            if (!await fileOpeningController.DownloadRequestItemAsync(_request.JobId))
            {
                Logger.LogError($"Failed to start the download of the item of the file opening request. JobId: {_request.JobId}",
                    "FileOpeningFailureWindow: Failed to start the item download");
            }
        }

        private void OnCloseClicked(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
