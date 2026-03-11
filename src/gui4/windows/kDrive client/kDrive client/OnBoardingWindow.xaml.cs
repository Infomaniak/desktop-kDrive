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

using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using System;

namespace Infomaniak.kDrive.OnBoarding
{
    public sealed partial class OnBoardingWindow : Window
    {
        private bool _isInitialized = false;
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private readonly ViewModels.Onboarding _onBoardingViewModel = new(App.ServiceProvider.GetRequiredService<ServerCommunication.Interfaces.IServerCommService>());
        private string _lottieRessourceKey = "Infomaniak.Custom.Animations.loader-stroke";
        public AppModel ViewModel { get { return _viewModel; } }
        public OnBoardingWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, 850, 530, false);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            LottiePlayer.ActualThemeChanged += LottiePlayer_ActualThemeChanged;
            UpdateLottieSource(_lottieRessourceKey, 130, 1);
            Closed += OnBoardingWindow_Closed;
            Activated += OnBoardingWindow_Activated;
        }

        private void OnBoardingWindow_Activated(object sender, WindowActivatedEventArgs args)
        {
            if (_isInitialized)
                return;
            ContentFrame.Navigate(typeof(Pages.Onboarding.WelcomePage), _onBoardingViewModel);
            _isInitialized = true;
        }

        private void OnBoardingWindow_Closed(object sender, WindowEventArgs args)
        {
            LottiePlayer.ActualThemeChanged -= LottiePlayer_ActualThemeChanged;
            _onBoardingViewModel.Dispose();
        }

        private void LottiePlayer_ActualThemeChanged(FrameworkElement sender, object args)
        {
            UpdateLottieSource(_lottieRessourceKey, null, 1);
        }

        public void UpdateLottieSource(string ressourceKey, int? height, int repeat = 0)
        {
            App.Current.Resources.TryGetValue(ressourceKey, out var sourceObj);
            if (sourceObj is string source)
            {
                _lottieRessourceKey = ressourceKey;
                LottiePlayer.UriSource = new System.Uri(source);
                LottiePlayer.PlayCount = repeat;
                if (height is not null)
                    LottiePlayer.Height = height.Value;
            }
            else
            {
                Logger.Log(Logger.Level.Warning, $"Lottie resource key '{ressourceKey}' not found or is not a string.");
            }
        }
    }
}
