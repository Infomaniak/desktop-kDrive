/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
