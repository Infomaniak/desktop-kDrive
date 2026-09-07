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
using System;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /// Hosts the "too many deletes" dialogs and forwards the user's answer to the
    /// <see cref="ManyDeletesController"/>, which owns all the decision logic.
    public sealed partial class ManyDeletesDialogHost : UserControl
    {
        private readonly ManyDeletesController _controller = App.ServiceProvider.GetRequiredService<AppModel>().ManyDeletesController;

        public ManyDeletesDialogHost()
        {
            InitializeComponent();
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        /// The notification currently displayed, exposed for x:Bind only.
        public ManyDeletesNotification? Notification => _controller.CurrentManyDeleteNotification;

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            _controller.ShowRequested += OnShowRequested;
            _controller.DismissRequested += OnDismissRequested;
            _controller.NotificationChanged += OnNotificationChanged;
            _controller.RequestDisplay();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            _controller.ShowRequested -= OnShowRequested;
            _controller.DismissRequested -= OnDismissRequested;
            _controller.NotificationChanged -= OnNotificationChanged;
        }

        private void OnNotificationChanged(object? sender, EventArgs e) => Bindings.Update();

        // The controller asks for the currently exposed notification to be displayed.
        private async void OnShowRequested(object? sender, EventArgs e) => await _controller.RunDisplayLoop(ShowDialogAsync);

        // The controller asks for the open dialog to be closed (a higher priority notification arrived).
        private void OnDismissRequested(object? sender, EventArgs e)
        {
            HardLimitDialog.Hide();
            SoftLimitDialog.Hide();
        }

        /// Shows the dialog matching the given notification and returns the action chosen by the user,
        /// or <c>null</c> when the dialog could not be shown.
        private async Task<ManyDeletesUserAction?> ShowDialogAsync(ManyDeletesNotification notification)
        {
            Bindings.Update();

            ContentDialog dialog = notification.IsHardLimit ? HardLimitDialog : SoftLimitDialog;
            dialog.XamlRoot = XamlRoot;
            
            Utility.BringCurrentWindowToFront();

            ContentDialogResult result;
            try
            {
                result = await dialog.ShowAsync();
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Warning, $"Failed to show the many deletes ContentDialog: {ex}");
                return null;
            }

            if (result == ContentDialogResult.None)
                return ManyDeletesUserAction.Dismissed; // Closed programmatically to give priority to another notification.

            if (notification.IsHardLimit)
                return result == ContentDialogResult.Secondary ? ManyDeletesUserAction.Continue : ManyDeletesUserAction.Revert;

            if (DoNotShowAgainCheckBox.IsChecked == true)
                return ManyDeletesUserAction.IgnoreNext;

            return result == ContentDialogResult.Secondary ? ManyDeletesUserAction.OpenTrash : ManyDeletesUserAction.Close;
        }

        // Prevents the dialogs from being light-dismissed (e.g. clicking outside or pressing Escape).
        // Programmatic Hide() calls are not affected as they do not raise a Closing event with a button result.
        private void Dialog_Closing(ContentDialog sender, ContentDialogClosingEventArgs e)
        {
            if (e.Result == ContentDialogResult.None && !_controller.IsDismissing)
                e.Cancel = true;
        }
    }
}
