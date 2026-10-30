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
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /// Hosts the file opening pop-up dialog and forwards the user's cancellation to the
    /// <see cref="FileOpeningController"/>, which tracks the state of the request reported by the server.
    public sealed partial class FileOpeningDialogHost : UserControl
    {
        private readonly FileOpeningController _controller = App.ServiceProvider.GetRequiredService<AppModel>().FileOpeningController;
        private FileOpeningRequest? _request;
        private bool _isDialogOpen;

        public FileOpeningDialogHost()
        {
            InitializeComponent();
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        /// The request currently displayed, exposed for x:Bind only.
        public FileOpeningRequest? Request => _request;

        public string CancelButtonLabel => Localizer.Instance.GetString("buttonCancel");

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            _controller.ShowRequested += OnShowRequested;
            _controller.DismissRequested += OnDismissRequested;
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            _controller.ShowRequested -= OnShowRequested;
            _controller.DismissRequested -= OnDismissRequested;
        }

        private void OnShowRequested(object? sender, FileOpeningStatusEventArgs e)
        {
            _request = e.Request;
            Bindings.Update();
            if (_isDialogOpen)
                return; // Already open for this request: the bindings refreshed its content.

            Utility.BringCurrentWindowToFront();
            _ = ShowDialogAsync();
        }

        private void OnDismissRequested(object? sender, FileOpeningStatusEventArgs e)
        {
            if (Request?.JobId != e.Request.JobId)
                return; // Another request is currently displayed: it must stay open.

            FileOpeningDialog.Hide();
        }

        private async Task ShowDialogAsync()
        {
            _isDialogOpen = true;
            try
            {
                FileOpeningDialog.XamlRoot = XamlRoot;
                await FileOpeningDialog.ShowAsync();
            }
            catch (Exception ex)
            {
                Logger.LogWarning($"Failed to show the file opening ContentDialog: {ex}", "Failed to show the file opening ContentDialog");
            }
            finally
            {
                _isDialogOpen = false;
            }
        }

        private async void OnCancelClicked(object sender, RoutedEventArgs e)
        {
            if (Request is null)
                return;

            // The server confirms the cancellation by reporting the Finished phase of the request, which closes the
            // dialog. Until then, the dialog stays open and the progress keeps being refreshed.
            await _controller.CancelRequestAsync(Request.JobId);
        }
    }
}
