using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reactive.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class UpdateManager : UISafeObservableObject
    {
        public enum ReleaseChannel
        {
            Production,
            Beta,
            Internal
        }

        private bool _isUpdateAvailable = false;
        private bool _autoUpdateEnabled = true;
        private ReleaseChannel _currentChannel = ReleaseChannel.Production;

        public bool IsUpdateAvailable
        {
            get => _isUpdateAvailable;
            set => SetPropertyInUIThread(ref _isUpdateAvailable, value);
        }

        public bool AutoUpdateEnabled
        {
            get => _autoUpdateEnabled;
            set => SetPropertyInUIThread(ref _autoUpdateEnabled, value);
        }

        public ReleaseChannel CurrentChannel
        {
            get => _currentChannel;
            set => SetPropertyInUIThread(ref _currentChannel, value);
        }
        public UpdateData? AvailableUpdate { get; set; } = null;

    }
}
