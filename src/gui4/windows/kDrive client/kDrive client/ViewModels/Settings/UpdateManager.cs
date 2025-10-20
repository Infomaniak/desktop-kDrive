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

        private bool _autoUpdateEnabled = true;
        private ReleaseChannel _currentChannel = ReleaseChannel.Production;
        private UpdateData? _updateData;

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
        public UpdateData? AvailableUpdate
        {
            get => _updateData;
            set => SetPropertyInUIThread(ref _updateData, value);
        }

       public UpdateManager()
        {
            AvailableUpdate = new UpdateData { Tag  ="4.0.0", BuildVersion="20251020"};
        }

    }
}
