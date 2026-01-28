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
        private bool _autoStart = false;
        private bool _moveToTrash = false;
        private NotificationsDisabled _notificationsDisabled;
        private Logger.Level _logLevel = Logger.Level.Extended;
        private bool _purgeOldLogs = true;
        private bool _logEnbaled = false;
        private bool _extendedLogEnabled = false;
        private ProxyConfig _proxyConfig = new ProxyConfig();
        private bool _matomoEnabled;
        private bool _sentryEnabled;
        public UpdateManager UpdateManager { get; } = new UpdateManager();
        public LogUploadManager LogUploadManager { get; } = new LogUploadManager();

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

        public async Task ChangeAutoStart(bool activated)
        {
            AutoStart = activated;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }
        public async Task ChangeNotificationsDisabled(NotificationsDisabled notificationsDisabled)
        {
            NotificationsDisabled = notificationsDisabled;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }
        public async Task ChangeMoveToTrash(bool activated)
        {
            MoveToTrash = activated;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }

        public async Task ChangeMatomoEnabled(bool enabled)
        {
            MatomoEnabled = enabled;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }
        public async Task ChangeSentryEnabled(bool enabled)
        {
            SentryEnabled = enabled;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }

        public async Task ChangeProxyType(ProxyType newType)
        {
            ProxyConfig.Type = newType;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }

        public async Task ChangeProxyConfiguration(string hostName, int port, bool needsAuth, string user, string pwd)
        {
            ProxyConfig.HostName = hostName;
            ProxyConfig.Port = port;
            ProxyConfig.NeedsAuth = needsAuth;
            ProxyConfig.User = user;
            ProxyConfig.Pwd = pwd;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }

        public async Task ChangeLogLevel(Logger.Level newLevel)
        {
            LogLevel = newLevel;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }
        public async Task ChangePurgeOldLog(bool enabled)
        {
            PurgeOldLogs = enabled;
            await App.ServiceProvider.GetRequiredService<IServerCommService>().SaveSettings(CancellationToken.None);
        }

        public async Task Refresh()
        {
            await App.ServiceProvider.GetRequiredService<IServerCommService>().RefreshSettings(CancellationToken.None);
        }
    }
}
