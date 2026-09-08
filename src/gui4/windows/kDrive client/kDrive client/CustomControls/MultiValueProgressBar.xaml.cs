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
using Microsoft.UI.Xaml.Media;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class MultiValueProgressBar : UserControl
    {
        public MultiValueProgressBar()
        {
            InitializeComponent();
        }


        private static readonly DependencyProperty Color1Property =
            DependencyProperty.Register("Color1", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value1Property =
            DependencyProperty.Register("Value1", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));


        private static readonly DependencyProperty Color2Property =
            DependencyProperty.Register("Color2", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value2Property =
            DependencyProperty.Register("Value2", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));

        private static readonly DependencyProperty Color3Property =
            DependencyProperty.Register("Color3", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Value3Property =
            DependencyProperty.Register("Value3", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata(0.0));

        public Brush Color1
        {
            get { return (Brush)GetValue(Color1Property); }
            set { SetValue(Color1Property, value); }
        }

        public long? Value1
        {
            get { return (long?)GetValue(Value1Property); }
            set { SetValue(Value1Property, value); }
        }

        public Brush Color2
        {
            get { return (Brush)GetValue(Color2Property); }
            set { SetValue(Color2Property, value); }
        }

        public long? Value2
        {
            get { return (long?)GetValue(Value2Property); }
            set { SetValue(Value2Property, value); }
        }

        public Brush Color3
        {
            get { return (Brush)GetValue(Color3Property); }
            set { SetValue(Color3Property, value); }
        }

        public long? Value3
        {
            get { return (long?)GetValue(Value3Property); }
            set { SetValue(Value3Property, value); }
        }

    }
}
