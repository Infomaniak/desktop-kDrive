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
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DriveStatusIndicator : UserControl
    {
        public DriveStatusIndicator()
        {
            this.InitializeComponent();
        }

        // DependencyProperty
        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public Brush IconForegroundBrush
        {
            get => (Brush)GetValue(IconForegroundBrushProperty);
            set => SetValue(IconForegroundBrushProperty, value);
        }

        public Brush IconBackgroundBrush
        {
            get => (Brush)GetValue(IconBackgroundBrushProperty);
            set => SetValue(IconBackgroundBrushProperty, value);
        }

        public Drive Drive
        {
            get => (Drive)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        public static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty DriveProperty = DependencyProperty.Register(nameof(DriveProperty), typeof(Drive), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty IconForegroundBrushProperty = DependencyProperty.Register(nameof(IconForegroundBrush), typeof(Brush), typeof(DriveStatusIndicator), new PropertyMetadata(null));
        public static readonly DependencyProperty IconBackgroundBrushProperty = DependencyProperty.Register(nameof(IconBackgroundBrush), typeof(Brush), typeof(DriveStatusIndicator), new PropertyMetadata(null));
    }
}
