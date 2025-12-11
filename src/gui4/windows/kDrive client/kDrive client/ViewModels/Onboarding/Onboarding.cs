using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Onboarding : UISafeObservableObject
    {
        private readonly IServerCommService _serverCommService;
        private OAuth2State _currentOAuth2State = OAuth2State.None;
        private User? _selectedUser = null;


        internal Onboarding(IServerCommService serverCommService)
        {
            _serverCommService = serverCommService;
        }

        public ObservableCollection<NewSync> NewSyncs { get; } = new();

        public OAuth2State CurrentOAuth2State
        {
            get => _currentOAuth2State;
            set => SetPropertyInUIThread(ref _currentOAuth2State, value);
        }

        public User? SelectedUser
        {
            get => _selectedUser;
            set => SetPropertyInUIThread(ref _selectedUser, value);
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
                Logger.Log(Logger.Level.Error, $"Authentication process failed {ex.Message}");
            }
        }

        public async Task<bool> FinishOnboarding()
        {
            if (SelectedUser == null)
            {
                Logger.Log(Logger.Level.Error, "No user selected to finish onboarding.");
                return false;
            }
            if (!NewSyncs.Any())
            {
                Logger.Log(Logger.Level.Warning, "No new syncs to set up during onboarding.");
                return false;
            }

            Logger.Log(Logger.Level.Info, $"Finishing onboarding for user {SelectedUser.Name} with {NewSyncs.Count} new syncs.");
            var commService = _serverCommService;
            foreach (var sync in NewSyncs)
            {
                Logger.Log(Logger.Level.Debug, $"Setting up new sync: LocalPath={sync.LocalPath}, RemotePath={sync.RemotePath}, Drive={sync.Drive.Name}");
                await _serverCommService.AddSync(sync, CancellationToken.None);
            }

            Logger.Log(Logger.Level.Info, $"Onboarding finished for user {SelectedUser.Name}.");
            return true;
        }
    }
}
