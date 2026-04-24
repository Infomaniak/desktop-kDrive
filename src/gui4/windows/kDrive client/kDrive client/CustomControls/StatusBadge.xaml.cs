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
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class StatusBadge : UserControl
    {
        public StatusBadge()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(StatusBadge), new PropertyMetadata(null));

        private double GetIconSize(double size) => size * 0.5;
    }
}
