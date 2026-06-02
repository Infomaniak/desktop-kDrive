namespace Infomaniak.kDrive.ViewModels
{
    public class ExclusionTemplate : UISafeObservableObject
    {
        private bool _warning;
        private bool _default; // True if the template is a default one, false if it's user-defined
        private string _template;
        private bool _isSelected;

        public ExclusionTemplate(string template, bool warning = true, bool isDefault = false)
        {
            _template = template;
            _warning = warning;
            _default = isDefault;
            _isSelected = false;
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

        public bool IsSelected
        {
            get => _isSelected;
            set => SetPropertyInUIThread(ref _isSelected, value);
        }
    }
}
