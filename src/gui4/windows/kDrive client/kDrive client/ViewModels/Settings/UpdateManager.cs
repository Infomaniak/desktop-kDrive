using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateManager : UISafeObservableObject
    {
        private bool _updateEnabled = false;
        private bool _autoUpdateEnabled = false;
        private VersionChannel _currentChannel = VersionChannel.Beta;
        private AppVersion? _availableUpdate;

        public bool UpdateEnabled
        {
            get => _updateEnabled;
            set => SetPropertyInUIThread(ref _updateEnabled, value);
        }

        public bool AutoUpdateEnabled
        {
            get => _autoUpdateEnabled;
            set => SetPropertyInUIThread(ref _autoUpdateEnabled, value);
        }

        public VersionChannel CurrentChannel
        {
            get => _currentChannel;
            set => SetPropertyInUIThread(ref _currentChannel, value);
        }
        public AppVersion? AvailableUpdate
        {
            get => _availableUpdate;
            set => SetPropertyInUIThread(ref _availableUpdate, value);
        }

        public static async Task<bool> StartUpdate()
        {
            return await App.ServiceProvider.GetRequiredService<IServerCommService>().StartUpdate(CancellationToken.None);
        }

        public async Task<bool> ChangeChannel(VersionChannel newChannel)
        {
            var previousChannel = CurrentChannel;
            CurrentChannel = newChannel;
            AvailableUpdate = null;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                CurrentChannel = previousChannel;
                return false;
            }

            return true;
        }

        public async Task<bool> ChangeAutoUpdate(bool activated)
        {
            AutoUpdateEnabled = activated;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                AutoUpdateEnabled = !activated;
                return false;
            }
            return true;
        }
    }
}
