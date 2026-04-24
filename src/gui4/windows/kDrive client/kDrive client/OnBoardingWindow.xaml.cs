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
        private DispatcherTimer? _columnAnimationTimer;
        private double _contentStartWidth;
        private double _lottieStartWidth;
        private double _contentTargetWidth;
        private double _lottieTargetWidth;
        private LottiePosition _targetPosition = LottiePosition.Right;
        private DateTimeOffset _animationStartTime;
        private const double _animationDurationMs = 300;
        private const double _contentColumnWeight = 1.63;
        private const double _lottieColumnWeight = 1.0;
        private const double _defaultContentWidthRatio = _contentColumnWeight / (_contentColumnWeight + _lottieColumnWeight);
        public AppModel ViewModel { get { return _viewModel; } }
        public OnBoardingWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            
            this.SetTitleBar(AppTitleBar);
            Utility.SetWindowProperties(this, 900, 600, false);
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
            ApplyLottiePosition(_targetPosition);

        }

        private async void OnBoardingWindow_Closed(object sender, WindowEventArgs args)
        {
            LottiePlayer.ActualThemeChanged -= LottiePlayer_ActualThemeChanged;
            LottiePlayer?.Cleanup();
            await _onBoardingViewModel.DisposeAsync();
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

        public enum LottiePosition
        {
            FullWindow,
            Right
        }
        public void SetLottiePosition(LottiePosition lottiePosition)
        {
            double totalWidth = ContentColumn.ActualWidth + LottieColumn.ActualWidth;
            if (double.IsNaN(totalWidth) || double.IsInfinity(totalWidth) || totalWidth <= 0)
            {
                ApplyLottiePosition(lottiePosition);
                return;
            }

            _columnAnimationTimer?.Stop();
            _contentStartWidth = ContentColumn.ActualWidth;
            _lottieStartWidth = LottieColumn.ActualWidth;
            _targetPosition = lottiePosition;

            switch (lottiePosition)
            {
                case LottiePosition.FullWindow:
                    _contentTargetWidth = 0;
                    _lottieTargetWidth = totalWidth;
                    break;
                case LottiePosition.Right:
                    _contentTargetWidth = totalWidth * _defaultContentWidthRatio;
                    _lottieTargetWidth = totalWidth * (1 - _defaultContentWidthRatio);
                    break;
                default:
                    return;
            }

            _animationStartTime = DateTimeOffset.Now;
            const double _animationFrameIntervalMs = 16; // ~60fps
            _columnAnimationTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(_animationFrameIntervalMs) };
            _columnAnimationTimer.Tick += OnColumnAnimationTick;
            _columnAnimationTimer.Start();
        }

        private void OnColumnAnimationTick(object? sender, object e)
        {
            double elapsed = (DateTimeOffset.Now - _animationStartTime).TotalMilliseconds;
            double t = Math.Clamp(elapsed / _animationDurationMs, 0.0, 1.0);
            double easedT = EaseInOutQuad(t);

            ContentColumn.Width = new GridLength(_contentStartWidth + (_contentTargetWidth - _contentStartWidth) * easedT, GridUnitType.Pixel);
            LottieColumn.Width = new GridLength(_lottieStartWidth + (_lottieTargetWidth - _lottieStartWidth) * easedT, GridUnitType.Pixel);

            if (t >= 1.0)
            {
                _columnAnimationTimer!.Stop();
                _columnAnimationTimer.Tick -= OnColumnAnimationTick;
                _columnAnimationTimer = null;
                ApplyLottiePosition(_targetPosition);
            }
        }

        private void ApplyLottiePosition(LottiePosition lottiePosition)
        {
            switch (lottiePosition)
            {
                case LottiePosition.FullWindow:
                    ContentColumn.Width = new GridLength(0, GridUnitType.Pixel);
                    LottieColumn.Width = new GridLength(1, GridUnitType.Star);
                    break;
                case LottiePosition.Right:
                    ContentColumn.Width = new GridLength(_defaultContentWidthRatio, GridUnitType.Star);
                    LottieColumn.Width = new GridLength(1 - _defaultContentWidthRatio, GridUnitType.Star);
                    break;
            }
        }

        private static double EaseInOutQuad(double t) =>
            t < 0.5 ? 2 * t * t : 1 - Math.Pow(-2 * t + 2, 2) / 2;
    }
}
