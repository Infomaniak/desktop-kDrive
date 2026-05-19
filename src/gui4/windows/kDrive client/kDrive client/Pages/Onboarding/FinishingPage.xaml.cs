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
using Infomaniak.kDrive.OnBoarding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using static Infomaniak.kDrive.OnBoarding.OnBoardingWindow;

namespace Infomaniak.kDrive.Pages.Onboarding
{
    public sealed partial class FinishingPage : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding? _onBoardingViewModel;
        public AppModel ViewModel { get { return _viewModel; } }

        public FinishingPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to FinishingPage - Initializing FinishPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "FinishingPage components initialized");
        }
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            if (e.Parameter is ViewModels.Onboarding obvm)
            {
                _onBoardingViewModel = obvm;

                if (_onBoardingViewModel.NewSyncs.Count == 0)
                {
                    Logger.Log(Logger.Level.Warning, "No drives selected to sync. User must select at least one drive.");
                    Frame.GoBack();
                    return;
                }
                if ((App.Current as App)?.CurrentWindow is OnBoardingWindow onBoardingWindow)
                    await onBoardingWindow.UpdateLottieSource(LottieTemplateKey.KDrive_SyncroFile);

                if (await _onBoardingViewModel.FinishOnboarding())
                    Frame.Navigate(typeof(FinishPage), _onBoardingViewModel);
                else
                {
                    Logger.Log(Logger.Level.Info, "Finishing onboarding failed. Returning to previous page.");
                    _onBoardingViewModel.NewSyncs.Clear();
                    Utility.ShowUnexpectedErrorTeachingTip();
                    Frame.GoBack();
                }
            }
            else
            {
                var errorMessage = "OnBoardingViewModel parameter missing when navigating to FinishingPage";
                Logger.Log(Logger.Level.Fatal, errorMessage);
                throw new Exception(errorMessage);
            }
        }
    }
}
