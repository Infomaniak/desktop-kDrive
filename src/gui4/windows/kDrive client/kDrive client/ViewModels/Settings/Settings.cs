using Infomaniak.kDrive.Types;
using System.Collections.ObjectModel;

namespace Infomaniak.kDrive.ViewModels
{
    public class Settings : UISafeObservableObject
    {

        private bool _startAtLogin = false;

        private readonly Language _currentLanguage = Language.SystemDefault; // The current language of the application. It can only be changed by restarting the application.
        private Language _selectedLanguage = Language.SystemDefault;
        private bool _selectLanguageMatchesCurrent = true;
        private readonly ObservableCollection<ExclusionTemplate> _exclusionTemplates = new ObservableCollection<ExclusionTemplate>();
        private bool _sentryEnabled = false;
        private bool _matomoEnabled = false;
        private Logger.Level _logLevel = Logger.Level.Extended;
        private bool _autoFlushLogs = true;
        private AppVersion? _appVersion;
        public Settings()
        {
            AppVersion = new AppVersion { Tag = "3.7.6", BuildVersion = "20250908" };
        }

        public UpdateManager UpdateManager { get; } = new UpdateManager();
        public NetworkSettings NetworkSettings { get; } = new NetworkSettings();

        public bool StartAtLogin
        {
            get => _startAtLogin;
            set => SetPropertyInUIThread(ref _startAtLogin, value);
        }

        public Language CurrentLanguage
        {
            get => _currentLanguage;
        }

        public Language SelectedLanguage
        {
            get => _selectedLanguage;
            set
            {
                if (SetPropertyInUIThread(ref _selectedLanguage, value) && value != CurrentLanguage)
                {
                    SetPropertyInUIThread(ref _selectLanguageMatchesCurrent, false);
                }
            }
        }

        public bool SelectLanguageMatchesCurrent
        {
            get => _selectLanguageMatchesCurrent;
        }

        public ObservableCollection<ExclusionTemplate> ExclusionTemplates
        {
            get => _exclusionTemplates;
        }

        public bool SentryEnabled
        {
            get => _sentryEnabled;
            set => SetPropertyInUIThread(ref _sentryEnabled, value);
        }

        public bool MatomoEnabled
        {
            get => _matomoEnabled;
            set => SetPropertyInUIThread(ref _matomoEnabled, value);
        }

        public Logger.Level LogLevel
        {
            get => _logLevel;
            set => SetPropertyInUIThread(ref _logLevel, value);
        }

        public bool AutoFlushLogs
        {
            get => _autoFlushLogs;
            set => SetPropertyInUIThread(ref _autoFlushLogs, value);
        }
        public AppVersion? AppVersion
        {
            get => _appVersion;
            set => SetPropertyInUIThread(ref _appVersion, value);
        }
    }
}
