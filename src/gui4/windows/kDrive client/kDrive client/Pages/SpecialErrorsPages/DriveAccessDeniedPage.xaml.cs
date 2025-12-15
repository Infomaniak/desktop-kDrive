/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.ComponentModel;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class DriveAccessDeniedPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;
        public DriveAccessDeniedPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to HomePage - Initializing HomePage components");
            InitializeComponent();
            Unloaded += (_, _) => DetachHandlers();
            Logger.Log(Logger.Level.Debug, "HomePage components initialized");
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            ViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            OnSelectedSyncChanged(null, new(null, ViewModel.SelectedSync));
            RedirectToHomePageIfNeeded();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachHandlers();
        }

        private void DetachHandlers()
        {
            ViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;

            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged -= OnSelectedSyncPropertyChanged;
        }

        private void OnSelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (e.OldValue is not null)
                e.OldValue.PropertyChanged -= OnSelectedSyncPropertyChanged;

            if (e.NewValue is not null)
                e.NewValue.PropertyChanged += OnSelectedSyncPropertyChanged;
        }

        private void OnSelectedSyncPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.SyncErrorState))
            {
                RedirectToHomePageIfNeeded();
            }
        }

        private void RedirectToHomePageIfNeeded()
        {
            if (ViewModel?.SelectedSync is null || ViewModel.SelectedSync.SyncErrorState != SyncErrorStates.AccessDenied)
            {
                DetachHandlers();
                AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(HomePage)));
            }
        }

        private async void RetryButton_Click(object sender, RoutedEventArgs e)
        {
            if (ViewModel?.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "");
                DetachHandlers();
                Frame.Navigate(typeof(HomePage));
            }

            await ViewModel.SelectedSync.Start();
        }
    }
}
