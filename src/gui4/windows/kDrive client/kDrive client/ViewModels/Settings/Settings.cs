using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Microsoft.Extensions.DependencyInjection;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Settings : UISafeObservableObject
    {
        private Language _language = Language.SystemDefault;
        private bool _restartRequiredForLanguageChange = false;
        private bool _autoStart = false;
        private bool _moveToTrash = false;
        private NotificationsDisabled _notificationsDisabled;
        private Logger.Level _logLevel = Logger.Level.Extended;
        private bool _purgeOldLogs = true;
        private bool _logEnbaled = false;
        private bool _extendedLogEnabled = false;
        private ProxyConfig _proxyConfig = new ProxyConfig();
        private bool _matomoEnabled = true;
        private bool _sentryEnabled = true;
        public UpdateManager UpdateManager { get; } = new UpdateManager();
        public LogUploadManager LogUploadManager { get; } = new LogUploadManager();

        public Language Language
        {
            get => _language;
            set
            {
                SetPropertyInUIThread(ref _language, value);
                Localizer.Instance.SetCulture(value);
            }
        }

        public bool RestartRequiredForLanguageChange
        {
            get => _restartRequiredForLanguageChange;
            set => SetPropertyInUIThread(ref _restartRequiredForLanguageChange, value);
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
            set
            {
                SetPropertyInUIThread(ref _logLevel, value);
                SetPropertyInUIThread(ref _logEnbaled, value != Logger.Level.None, nameof(LogEnabled));
                SetPropertyInUIThread(ref _extendedLogEnabled, value == Logger.Level.Extended, nameof(ExtendedLogEnbaled));
            }
        }

        public bool LogEnabled => _logEnbaled;
        public bool ExtendedLogEnbaled => _extendedLogEnabled;

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
        public AppVersion AppVersion { get; } = AppVersion.Current();

        public bool MatomoEnabled
        {
            get => _matomoEnabled;
            set => SetPropertyInUIThread(ref _matomoEnabled, value);
        }

        public bool SentryEnabled
        {
            get => _sentryEnabled;
            set => SetPropertyInUIThread(ref _sentryEnabled, value);
        }

        public async Task<bool> ChangeAutoStart(bool activated)
        {
            if (AutoStart == activated)
                return true;

            AutoStart = activated;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                AutoStart = !activated;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeNotificationsDisabled(NotificationsDisabled notificationsDisabled)
        {
            if (NotificationsDisabled == notificationsDisabled)
                return true;

            var previousState = NotificationsDisabled;
            NotificationsDisabled = notificationsDisabled;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                NotificationsDisabled = previousState;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeMoveToTrash(bool activated)
        {
            if (MoveToTrash == activated)
                return true;

            MoveToTrash = activated;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                MoveToTrash = !activated;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeMatomoEnabled(bool enabled)
        {
            if (MatomoEnabled == enabled)
                return true;

            MatomoEnabled = enabled;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                MatomoEnabled = !enabled;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeSentryEnabled(bool enabled)
        {
            if (SentryEnabled == enabled)
                return true;

            SentryEnabled = enabled;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                SentryEnabled = !enabled;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeProxyType(ProxyType newType)
        {
            if (ProxyConfig.Type == newType)
                return true;

            var previousType = ProxyConfig.Type;
            ProxyConfig.Type = newType;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                ProxyConfig.Type = previousType;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeProxyConfiguration(string hostName, int port, bool needsAuth, string user, string pwd)
        {
            if (ProxyConfig.HostName == hostName && ProxyConfig.Port == port && ProxyConfig.NeedsAuth == needsAuth && ProxyConfig.User == user && ProxyConfig.Pwd == pwd)
                return true;

            var PreviousConfig = ProxyConfig.Clone();
            ProxyConfig.HostName = hostName;
            ProxyConfig.Port = port;
            ProxyConfig.NeedsAuth = needsAuth;
            ProxyConfig.User = user;
            ProxyConfig.Pwd = pwd;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                ProxyConfig.HostName = PreviousConfig.HostName;
                ProxyConfig.Port = PreviousConfig.Port;
                ProxyConfig.NeedsAuth = PreviousConfig.NeedsAuth;
                ProxyConfig.User = PreviousConfig.User;
                ProxyConfig.Pwd = PreviousConfig.Pwd;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeLogLevel(Logger.Level newLevel)
        {
            if (LogLevel == newLevel)
                return true;

            var previousLogLevel = LogLevel;
            LogLevel = newLevel;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                LogLevel = previousLogLevel;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangePurgeOldLog(bool enabled)
        {
            if (PurgeOldLogs == enabled)
                return true;

            PurgeOldLogs = enabled;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                PurgeOldLogs = !enabled;
                return false;
            }
            return true;
        }

        public async Task<bool> ChangeLanguage(Language newLanguage)
        {
            if (Language == newLanguage)
                return true;

            var previousLanguage = Language;
            Language = newLanguage;
            if (!await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None))
            {
                Language = previousLanguage;
                return false;
            }
            RestartRequiredForLanguageChange = true;
            return true;
        }
    }
}
