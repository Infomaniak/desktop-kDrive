using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.ViewModels
{
    public class ExclusionTemplate : UISafeObservableObject
    {
        private bool _warning;
        private bool _default; // True if the template is a default one, false if it's user-defined
        private string _template;

        public ExclusionTemplate(string template, bool isActive = true, bool isDefault = false)
        {
            _template = template;
            _warning = isActive;
            _default = isDefault;
        }

        public bool Warning
        {
            get => _warning;
            set => SetPropertyInUIThread(ref _warning, value);
        }

        public bool Default
        {
            get => _default;
            set => SetPropertyInUIThread(ref _default, value);
        }

        public string Template
        {
            get => _template;
            set => SetPropertyInUIThread(ref _template, value);
        }
    }
}
