using Infomaniak.kDrive.ServerCommunication;
using Infomaniak.kDrive.Types;
using System.Collections.ObjectModel;

namespace Infomaniak.kDrive.ViewModels
{
    public class ProxyConfig : UISafeObservableObject
    {
        private ProxyType _type = ProxyType.None;
        private string _hostName = "";
        private int _port = 0;
        private bool _needsAuth = false;
        private string _user = "";
        private string _pwd = "";
        public ProxyType Type
        {
            get => _type;
            set => SetPropertyInUIThread(ref _type, value);
        }
        public string HostName
        {
            get => _hostName;
            set => SetPropertyInUIThread(ref _hostName, value);
        }
        public int Port
        {
            get => _port;
            set => SetPropertyInUIThread(ref _port, value);
        }
        public bool NeedsAuth
        {
            get => _needsAuth;
            set => SetPropertyInUIThread(ref _needsAuth, value);
        }
        public string User
        {
            get => _user;
            set => SetPropertyInUIThread(ref _user, value);
        }
        public string Pwd
        {
            get => _pwd;
            set => SetPropertyInUIThread(ref _pwd, value);
        }
    }

    public class Settings : UISafeObservableObject
    {
        public Settings()
        {
            AppVersion = new AppVersion { Tag = "3.7.6", BuildVersion = "20250908" };
        }
        private Language _language = Language.SystemDefault;
        private bool _autoStart = false;
        private bool _moveToTrash = false;
        private NotificationsDisabled _notificationsDisabled;
        private Logger.Level _logLevel = Logger.Level.Extended;
        private bool _purgeOldLogs = true;
        private ProxyConfig _proxyConfig = new ProxyConfig();
        public long? _bigFolderSizeLimit;
        public bool? _showShortcuts;

        private AppVersion? _appVersion;
        public UpdateManager UpdateManager { get; } = new UpdateManager();

        public Language Language
        {
            get => _language;
            set => SetPropertyInUIThread(ref _language, value);
        }

        public bool AutoStart
        {
            get => _autoStart;
            set => SetPropertyInUIThread(ref _autoStart, value);
        }
        public bool MoveToTrash
        {
            get => _moveToTrash;
            set => SetPropertyInUIThread(ref _moveToTrash, value);
        }
        public NotificationsDisabled NotificationsDisabled
        {
            get => _notificationsDisabled;
            set => SetPropertyInUIThread(ref _notificationsDisabled, value);
        }
        public Logger.Level LogLevel
        {
            get => _logLevel;
            set => SetPropertyInUIThread(ref _logLevel, value);
        }
        public bool PurgeOldLogs
        {
            get => _purgeOldLogs;
            set => SetPropertyInUIThread(ref _purgeOldLogs, value);
        }
        public ProxyConfig ProxyConfig
        {
            get => _proxyConfig;
            set => SetPropertyInUIThread(ref _proxyConfig, value);
        }
        public bool? ShowShortcuts
        {
            get => _showShortcuts;
            set => SetPropertyInUIThread(ref _showShortcuts, value);
        }
        public AppVersion? AppVersion
        {
            get => _appVersion;
            set => SetPropertyInUIThread(ref _appVersion, value);
        }
    }
}
