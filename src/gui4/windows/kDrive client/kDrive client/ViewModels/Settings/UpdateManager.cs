using CommunityToolkit.Mvvm.Input;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateManager : UISafeObservableObject
    {
        private bool _autoUpdateEnabled = true;
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

        public UpdateManager()
        {
            StartUpdateCommand = new AsyncRelayCommand(StartUpdate);
        }
        public IAsyncRelayCommand StartUpdateCommand { get; }
        public async Task<bool> StartUpdate()
        {
            await App.ServiceProvider.GetRequiredService<IServerCommService>().StartUpdate(default);
            return true;
        }

        public async Task ChangeChannel(VersionChannel newChannel)
        {
            await App.ServiceProvider.GetRequiredService<IServerCommService>().ChangeUpdaterChannel(newChannel, default);
        }
    }
}
