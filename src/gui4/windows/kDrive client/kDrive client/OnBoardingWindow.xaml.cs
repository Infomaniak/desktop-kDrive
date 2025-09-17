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

using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.ServerCommunication;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Input;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reactive.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.OnBoarding
{
    public sealed partial class OnBoardingWindow : Window
    {
        private readonly AppModel _viewModel = ((App)Application.Current).Data;
        private OnboardingViewModel _onBoardingViewModel = new OnboardingViewModel();

        public AppModel ViewModel { get { return _viewModel; } }
        public OnBoardingWindow()
        {
            InitializeComponent();
            this.ExtendsContentIntoTitleBar = true;  // enable custom titlebar
            this.SetTitleBar(AppTitleBar);

            if (ViewModel.Users.Any())
            {
                // TODO: Go directly to Drive selection
            }
            ContentFrame.Navigate(typeof(Pages.Onboarding.WelcomePage), _onBoardingViewModel);
        }
    }

    public class OnboardingViewModel : ObservableObject
    {
        public enum OAuth2State
        {
            None,
            WaitingForUserAction,
            ProcessingResponse,
            Success,
            Error
        }
        private OAuth2State _currentOAuth2State = OAuth2State.None;
        public OAuth2State CurrentOAuth2State
        {
            get => _currentOAuth2State;
            set
            {
                SetProperty(ref _currentOAuth2State, value);
            }
        }

        public async Task ConnectUser(CancellationToken cancelationToken)
        {
            CurrentOAuth2State = OAuth2State.WaitingForUserAction;
            try
            {
                string newUserToken = await OAuthHelper.GetToken(cancelationToken);
                if (newUserToken != "")
                {
                    Logger.Log(Logger.Level.Debug, "Successfully obtained user token.");
                    CurrentOAuth2State = OAuth2State.ProcessingResponse;
                    // TODO: Add user to the app
                    await Task.Delay(3000, cancelationToken); // Simulate processing time
                    CurrentOAuth2State = OAuth2State.Success;
                }
                else
                {
                    CurrentOAuth2State = OAuth2State.Error;
                    Logger.Log(Logger.Level.Warning, "Authentication process failed");
                }
            }
            catch (OperationCanceledException)
            {
                CurrentOAuth2State = OAuth2State.Error;
                Logger.Log(Logger.Level.Warning, "Authentication process canceled by user.");
            }
        }
    }
}
