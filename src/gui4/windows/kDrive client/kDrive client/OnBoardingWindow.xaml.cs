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
using System.Linq;
using System.Reactive.Linq;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.OnBoarding
{
    public sealed partial class OnBoardingWindow : Window
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private ViewModels.Onboarding _onBoardingViewModel = new(App.ServiceProvider.GetRequiredService<ServerCommunication.Interfaces.IServerCommService>());
        private string _lottieRessourceKey = "Infomaniak.Custom.Animations.loader-stroke";
        public AppModel ViewModel { get { return _viewModel; } }
        public OnBoardingWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, 850, 530, false);
            AppWindow.TitleBar.PreferredTheme = Microsoft.UI.Windowing.TitleBarTheme.UseDefaultAppMode;
            ContentFrame.Navigate(typeof(Pages.Onboarding.WelcomePage), _onBoardingViewModel);
            LottiePlayer.ActualThemeChanged += (s, e) =>
            {
                UpdateLottieSource(_lottieRessourceKey);
            };
            UpdateLottieSource(_lottieRessourceKey);

            Closed += OnBoardingWindow_Closed;
        }

        private void OnBoardingWindow_Closed(object sender, WindowEventArgs args)
        {
            _onBoardingViewModel.Dispose();
        }

        public void UpdateLottieSource(string ressourceKey, int? height = null)
            {
            App.Current.Resources.TryGetValue(ressourceKey, out var sourceObj);
            if (sourceObj is string source)
            {
                _lottieRessourceKey = ressourceKey;
                LottiePlayer.UriSource = new System.Uri(source);
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
