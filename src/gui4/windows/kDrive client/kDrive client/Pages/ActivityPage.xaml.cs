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
using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Linq;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class ActivityPage : Page
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public ActivityPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ActivityPage - Initializing ActivityPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ActivityPage components initialized");
            UpdateTitleTemplate();
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            if (ViewModel.SelectedSync is null)
            {
                AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(SettingsPage)));
            }
            else
            {
                ViewModel.SelectedSyncChanged += ViewModel_SelectedSyncChanged;
                ViewModel.SelectedSync.PropertyChanged += ViewModel_SelectedSync_PropertyChanged;
                _analyticsService.TrackPageView(Analytics.Keys.Category.ActivityPage);
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachEventHandlers();
        }

        private void ViewModel_SelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            if (e.OldValue != null)
            {
                e.OldValue.PropertyChanged -= ViewModel_SelectedSync_PropertyChanged;
            }
            if (e.NewValue != null)
            {
                e.NewValue.PropertyChanged += ViewModel_SelectedSync_PropertyChanged;
            }
            UpdateTitleTemplate();
        }


        private void DetachEventHandlers()
        {
            ViewModel.SelectedSyncChanged -= ViewModel_SelectedSyncChanged;
            if (ViewModel.SelectedSync is not null)
            {
                ViewModel.SelectedSync.PropertyChanged -= ViewModel_SelectedSync_PropertyChanged;
            }
        }

        private void ViewModel_SelectedSync_PropertyChanged(object? sender, System.ComponentModel.PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Sync.SyncStatus))
                UpdateTitleTemplate();
        }

        private void UpdateTitleTemplate()
        {
            switch (ViewModel.SelectedSync?.SyncStatus)
            {
                case SyncStatus.Running or SyncStatus.Paused or SyncStatus.PauseAsked: /* Treat Paused & PauseAsked as In Progress because the sync will resume automatically */
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["InProgressTitleTemplate"];
                    break;
                case SyncStatus.Idle:
                    if (!ViewModel.SelectedSync.SyncActivities.Any())
                        TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["NoActivityTitleTemplate"];
                    else
                        TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["UpToDateTitleTemplate"];
                    break;
                case SyncStatus.Offline:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["OfflineTitleTemplate"];
                    break;
                case SyncStatus.Stopped:
                case SyncStatus.Error:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["InPauseTitleTemplate"];
                    break;
                default:
                    TitleContentControl.ContentTemplate = (DataTemplate)this.Resources["LoadingTitleTemplate"];
                    break;
            }
        }
        private void ShowIncomingActivityComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded) return;
            ComboBox? comboBox = sender as ComboBox;
            if (comboBox is null) return;

            if (comboBox.SelectedIndex == 0)
                _analyticsService.TrackClick(Analytics.Keys.Category.ActivityPage, Analytics.Keys.EventName.ShowMyActivities);
            else 
                _analyticsService.TrackClick(Analytics.Keys.Category.ActivityPage, Analytics.Keys.EventName.ShowAllActivities);

        }
    }
}
