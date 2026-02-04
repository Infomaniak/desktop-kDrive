using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateManager : UISafeObservableObject
    {
        private bool _autoUpdateEnabled = false;
        private VersionChannel _currentChannel = VersionChannel.Beta;
        private AppVersion? _updateData;

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
            get => _updateData;
            set => SetPropertyInUIThread(ref _updateData, value);
        }

        public static async Task<bool> StartUpdate()
        {
            return await App.ServiceProvider.GetRequiredService<IServerCommService>().StartUpdate(CancellationToken.None);
        }

        public static async Task<bool> ChangeChannel(VersionChannel newChannel)
        {
            return await App.ServiceProvider.GetRequiredService<IServerCommService>().ChangeUpdaterChannel(newChannel, CancellationToken.None);
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
