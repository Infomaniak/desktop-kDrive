using CommunityToolkit.Mvvm.ComponentModel;
using Infomaniak.kDrive.Types;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.ViewModels
{
    internal class Settings : ObservableObject
    {

        private bool _startAtLogin = false;

        private readonly Language _currentLanguage = Language.SystemDefault; // The current language of the application. It can only be changed by restarting the application.
        private Language _selectedLanguage = Language.SystemDefault;
        private bool _selectLanguageMatchesCurrent = true;

        public Settings()
        {

        }

        public UpdateManager UpdateManager { get; } = new UpdateManager();

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



    }
}
