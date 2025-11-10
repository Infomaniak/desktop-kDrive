using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Infomaniak.kDrive.Types;

namespace Infomaniak.kDrive.ViewModels
{
    public class Onboarding : ObservableObject
    {
        
        private OAuth2State _currentOAuth2State = OAuth2State.None;

        private User? _selectedUser = null;
        public OAuth2State CurrentOAuth2State
        {
            get => _currentOAuth2State;
            set
            {
                SetProperty(ref _currentOAuth2State, value);
            }
        }

        public User? SelectedUser
        {
            get => _selectedUser;
            set
            {
                SetProperty(ref _selectedUser, value);
            }
        }

        public ObservableCollection<Sync> NewSyncs { get; } = new();

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
                    SelectedUser = (App.Current as App)?.Data.Users.FirstOrDefault(u => u.IsConnected);
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

        public async Task FinishOnboarding()
        {
            if (SelectedUser == null)
            {
                Logger.Log(Logger.Level.Error, "No user selected to finish onboarding.");
                return;
            }
            // Simulate some finalization work
            await Task.Delay(5000);
            Logger.Log(Logger.Level.Info, $"Onboarding finished for user {SelectedUser.Name}.");
        }
    }
}
