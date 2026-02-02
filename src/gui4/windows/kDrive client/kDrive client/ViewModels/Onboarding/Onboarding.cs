using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Onboarding : UISafeObservableObject, IDisposable
    {
        private readonly IServerCommService _serverCommService;
        private OAuth2State _currentOAuth2State = OAuth2State.None;
        private User? _selectedUser = null;
        private Task? _driveAvailableWatcherTask;
        private CancellationTokenSource? _driveAvailableWatcherCts;

        internal Onboarding(IServerCommService serverCommService)
        {
            _serverCommService = serverCommService;
        }

        public event EventHandler? DrivesAvailable;

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

        public void Dispose()
        {
            _ = StopDriveAvailabilityWatcherAsync();
        }

        public void StartDriveAvailabilityWatcher()
        {
            if (_driveAvailableWatcherTask is not null && !_driveAvailableWatcherTask.IsCompleted)
                return;

            Logger.Log(Logger.Level.Info, "Starting drive availability watcher task");
            _driveAvailableWatcherCts = new CancellationTokenSource();
            _driveAvailableWatcherTask = WatchAvailableDrives(_driveAvailableWatcherCts.Token);
        }

        public async Task StopDriveAvailabilityWatcherAsync()
        {
            if (_driveAvailableWatcherTask is null)
                return;

            Logger.Log(Logger.Level.Info, "Cancelling drive availability watcher task");
            if (_driveAvailableWatcherCts is not null)
            {
                await _driveAvailableWatcherCts.CancelAsync();
            }

            try
            {
                await _driveAvailableWatcherTask;
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Drive availability watcher task cancelled successfully");
            }
            finally
            {
                _driveAvailableWatcherTask = null;
                _driveAvailableWatcherCts?.Dispose();
                _driveAvailableWatcherCts = null;
            }
        }

        private async Task WatchAvailableDrives(CancellationToken cancellationToken)
        {
            try
            {
                while (!cancellationToken.IsCancellationRequested)
                {
                    var drivesFound = await CheckAvailableDrives(cancellationToken);
                    if (drivesFound)
                    {
                        return;
                    }

                    await Task.Delay(5000, cancellationToken); // Check every 5 seconds
                }
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Info, "Drive availability watcher task cancelled");
            }
        }

        private async Task<bool> CheckAvailableDrives(CancellationToken cancellationToken)
        {
            if (SelectedUser is null)
            {
                Logger.Log(Logger.Level.Warning, "SelectedUser is null - Cannot check available drives");
                return false;
            }

            await SelectedUser.RefreshAvailableDrives(cancellationToken);
            if (!cancellationToken.IsCancellationRequested && SelectedUser.AllDrives.Any())
            {
                Logger.Log(Logger.Level.Info, "Drives found for user");
                DrivesAvailable?.Invoke(this, EventArgs.Empty);
                return true;
            }

            return false;
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
                    if(user is null || user.DbId == 0)
                    {
                        CurrentOAuth2State = OAuth2State.Error;
                        Logger.Log(Logger.Level.Warning, "Authentication process failed: unable to add or relog user.");
                        return;
                    }
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
            foreach (NewSync sync in NewSyncs)
            {
                Logger.Log(Logger.Level.Debug, $"Setting up new sync: LocalPath={sync.LocalPath}, RemotePath={sync.RemotePath}, Drive={sync?.Drive?.Name ?? "unknown"}");
                await _serverCommService.AddSync(sync!, CancellationToken.None);
            }

            Logger.Log(Logger.Level.Info, $"Onboarding finished for user {SelectedUser.Name}.");
            return true;
        }
    }
}
