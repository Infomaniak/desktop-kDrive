using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class ExclusionTemplate : ObservableObject
    {
        private bool _isActive;
        private bool _isDefault; // True if the template is a default one, false if it's user-defined
        private string _template;

        public ExclusionTemplate(string template, bool isActive = true, bool isDefault = false)
        {
            _template = template;
            _isActive = isActive;
            _isDefault = isDefault;
        }

        public bool IsActive
        {
            get => _isActive;
            set => SetProperty(ref _isActive, value);
        }

        public bool IsDefault
        {
            get => _isDefault;
            set => SetProperty(ref _isDefault, value);
        }

        public string Template
        {
            get => _template;
            set => SetProperty(ref _template, value);
        }
    }
}
