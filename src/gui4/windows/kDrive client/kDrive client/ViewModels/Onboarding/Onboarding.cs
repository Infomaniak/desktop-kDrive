using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.ServerCommunication;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Onboarding : UISafeObservableObject
    {
        private readonly IServerCommService _serverCommService;

        private OAuth2State _currentOAuth2State = OAuth2State.None;

        private User? _selectedUser = null;
        public OAuth2State CurrentOAuth2State
        {
            get => _currentOAuth2State;
            set
            {
                SetPropertyInUIThread(ref _currentOAuth2State, value);
            }
        }

        public User? SelectedUser
        {
            get => _selectedUser;
            set
            {
                SetPropertyInUIThread(ref _selectedUser, value);
            }
        }

        public ObservableCollection<Sync> NewSyncs { get; } = new();

        internal Onboarding(IServerCommService serverCommService)
        {
            _serverCommService = serverCommService;
        }
        public async Task ConnectUser(CancellationToken cancelationToken)
        {
            CurrentOAuth2State = OAuth2State.WaitingForUserAction;
            try
            {
                var OAutCodes = await OAuthHelper.GetCode(cancelationToken);
                if (OAutCodes.Code != "")
                {
                    Logger.Log(Logger.Level.Debug, "Successfully obtained user code.");
                    CurrentOAuth2State = OAuth2State.ProcessingResponse;
                    User? user = await _serverCommService.AddOrRelogUser(OAutCodes.Code, OAutCodes.CodeVerifier, cancelationToken);
                    SelectedUser = user;
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
            catch (Exception ex)
            {
                CurrentOAuth2State = OAuth2State.Error;
                Logger.Log(Logger.Level.Warning, $"Authentication process failed {ex.Message}");
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
