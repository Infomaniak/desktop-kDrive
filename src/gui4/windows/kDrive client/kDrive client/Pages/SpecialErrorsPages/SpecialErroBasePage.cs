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

using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Pages
{
    public partial class SpecialErroBasePage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private readonly List<SyncErrorStates> _handledErrorStates;
        public AppModel ViewModel => _viewModel;
        public SpecialErroBasePage(List<SyncErrorStates> syncErrorStates)
        {
            _handledErrorStates = syncErrorStates;
            Loaded += SpecialErroBasePage_Loaded;
        }

        private void SpecialErroBasePage_Loaded(object sender, RoutedEventArgs e)
        {
            // Remove all the previous pages in the back stack that are of a type derived from SpecialErroBasePage and the page -1 in the back stack to prevent navigation to error state by back navigation
            if (Frame?.BackStackDepth > 0)
            {
                Logger.Log(Logger.Level.Info, "Removing previous page from back stack to prevent navigation to error state");
                for (int i = Frame.BackStackDepth - 1; i >= 0; i--)
                {
                    if (Frame.BackStack[i].SourcePageType == typeof(SpecialErroBasePage) || Frame.BackStack[i].SourcePageType.IsSubclassOf(typeof(SpecialErroBasePage)))
                    {
                        Logger.Log(Logger.Level.Info, $"Removing page of type {Frame.BackStack[i].SourcePageType} from back stack to prevent navigation to error state");
                        Frame.BackStack.RemoveAt(i);
                    }
                }
                Frame.BackStack.RemoveAt(Frame.BackStackDepth - 1);
            }
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            ViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged += OnSelectedSyncPropertyChanged;
            RedirectToHomePageIfNeeded();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            DetachHandlers();
        }

        protected void DetachHandlers()
        {
            ViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;

            if (ViewModel.SelectedSync is not null)
                ViewModel.SelectedSync.PropertyChanged -= OnSelectedSyncPropertyChanged;
        }

        private void OnSelectedSyncChanged(object? sender, AppModel.SelectedSyncChangedEventArgs e)
        {
            AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(HomePage)));
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
            if (ViewModel?.SelectedSync is null || !_handledErrorStates.Contains(ViewModel.SelectedSync.SyncErrorState))
            {
                DetachHandlers();
                AppModel.UIThreadDispatcher.TryEnqueue(() => Frame.Navigate(typeof(HomePage)));
            }
        }

        protected async Task RestartSync()
        {
            if (ViewModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "No selected sync found - Navigating to HomePage");
                DetachHandlers();
                Frame.Navigate(typeof(HomePage));
                return;
            }

            if (!await ViewModel.SelectedSync.Start())
            {
                Logger.Log(Logger.Level.Error, "Failed to start sync.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }
    }
}
