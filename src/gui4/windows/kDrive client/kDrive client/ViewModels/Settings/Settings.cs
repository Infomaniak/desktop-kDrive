using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive;
using Infomaniak.kDrive.Types;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class Settings : ObservableObject
    {

        private bool _startAtLogin = false;

        private readonly Language _currentLanguage = Language.SystemDefault; // The current language of the application. It can only be changed by restarting the application.
        private Language _selectedLanguage = Language.SystemDefault;
        private bool _selectLanguageMatchesCurrent = true;
        private readonly ObservableCollection<ExclusionTemplate> _exclusionTemplates = new ObservableCollection<ExclusionTemplate>();
        private bool _sentryEnabled = false;
        private bool _matomoEnabled = false;
        private Logger.Level _logLevel = Logger.Level.Info;
        private bool _autoFlushLogs = true;

        public Settings() { }

        public UpdateManager UpdateManager { get; } = new UpdateManager();
        public NetworkSettings NetworkSettings { get; } = new NetworkSettings();

        public bool StartAtLogin
        {
            get => _startAtLogin;
            set => SetProperty(ref _startAtLogin, value);
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
                if (SetProperty(ref _selectedLanguage, value) && value != CurrentLanguage)
                {
                    SetProperty(ref _selectLanguageMatchesCurrent, false);
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
            set => SetProperty(ref _sentryEnabled, value);
        }

        public bool MatomoEnabled
        {
            get => _matomoEnabled;
            set => SetProperty(ref _matomoEnabled, value);
        }

        public Logger.Level LogLevel
        {
            get => _logLevel;
            set => SetProperty(ref _logLevel, value);
        }

        public bool AutoFlushLogs
        {
            get => _autoFlushLogs;
            set => SetProperty(ref _autoFlushLogs, value);
        }
    }
}
