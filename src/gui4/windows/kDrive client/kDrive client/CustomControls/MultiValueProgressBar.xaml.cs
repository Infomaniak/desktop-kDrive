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
using System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class MultiValueProgressBar : UserControl
    {
        public MultiValueProgressBar()
        {
            InitializeComponent();
        }

        public GridLength GetSectionWidth(Visibility visibility, Brush? color, long? value, long? value1, long? value2, long? value3, long? value4)
        {
            if (visibility == Visibility.Collapsed || color is null)
                return new GridLength(0, GridUnitType.Star);

            long total = Math.Max(0, value1 ?? 0) + Math.Max(0, value2 ?? 0) + Math.Max(0, value3 ?? 0) + Math.Max(0, value4 ?? 0);
            return new GridLength(Math.Max(Math.Max(0, value ?? 0), total / 100.0), GridUnitType.Star);
        }


        private static readonly DependencyProperty Color1Property =
            DependencyProperty.Register("Color1", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Color2Property =
            DependencyProperty.Register("Color2", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Color3Property =
            DependencyProperty.Register("Color3", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));

        private static readonly DependencyProperty Color4Property =
            DependencyProperty.Register("Color4", typeof(Brush), typeof(MultiValueProgressBar), new PropertyMetadata(null));


        private static readonly DependencyProperty Value1Property =
            DependencyProperty.Register("Value1", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata((long)0));

        private static readonly DependencyProperty Value2Property =
            DependencyProperty.Register("Value2", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata((long)0));

        private static readonly DependencyProperty Value3Property =
            DependencyProperty.Register("Value3", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata((long)0));

        private static readonly DependencyProperty Value4Property =
            DependencyProperty.Register("Value4", typeof(long?), typeof(MultiValueProgressBar), new PropertyMetadata((long)0));


        private static readonly DependencyProperty Visibility1Property =
            DependencyProperty.Register("Visibility1", typeof(Visibility), typeof(MultiValueProgressBar), new PropertyMetadata(Visibility.Visible));

        private static readonly DependencyProperty Visibility2Property =
            DependencyProperty.Register("Visibility2", typeof(Visibility), typeof(MultiValueProgressBar), new PropertyMetadata(Visibility.Visible));

        private static readonly DependencyProperty Visibility3Property =
            DependencyProperty.Register("Visibility3", typeof(Visibility), typeof(MultiValueProgressBar), new PropertyMetadata(Visibility.Visible));

        private static readonly DependencyProperty Visibility4Property =
            DependencyProperty.Register("Visibility4", typeof(Visibility), typeof(MultiValueProgressBar), new PropertyMetadata(Visibility.Visible));


        private static readonly DependencyProperty IsLoading1Property =
            DependencyProperty.Register("IsLoading1", typeof(bool), typeof(MultiValueProgressBar), new PropertyMetadata(false));

        private static readonly DependencyProperty IsLoading2Property =
            DependencyProperty.Register("IsLoading2", typeof(bool), typeof(MultiValueProgressBar), new PropertyMetadata(false));

        private static readonly DependencyProperty IsLoading3Property =
            DependencyProperty.Register("IsLoading3", typeof(bool), typeof(MultiValueProgressBar), new PropertyMetadata(false));

        private static readonly DependencyProperty IsLoading4Property =
            DependencyProperty.Register("IsLoading4", typeof(bool), typeof(MultiValueProgressBar), new PropertyMetadata(false));


        public bool IsLoading1
        {
            get => (bool)GetValue(IsLoading1Property);
            set => SetValue(IsLoading1Property, value);
        }

        public bool IsLoading2
        {
            get => (bool)GetValue(IsLoading2Property);
            set => SetValue(IsLoading2Property, value);
        }

        public bool IsLoading3
        {
            get => (bool)GetValue(IsLoading3Property);
            set => SetValue(IsLoading3Property, value);
        }

        public bool IsLoading4
        {
            get => (bool)GetValue(IsLoading4Property);
            set => SetValue(IsLoading4Property, value);
        }

        public Visibility Visibility1
        {
            get => (Visibility)GetValue(Visibility1Property);
            set => SetValue(Visibility1Property, value);
        }

        public Visibility Visibility2
        {
            get => (Visibility)GetValue(Visibility2Property);
            set => SetValue(Visibility2Property, value);
        }

        public Visibility Visibility3
        {
            get => (Visibility)GetValue(Visibility3Property);
            set => SetValue(Visibility3Property, value);
        }

        public Visibility Visibility4
        {
            get => (Visibility)GetValue(Visibility4Property);
            set => SetValue(Visibility4Property, value);
        }


        public Brush Color1
        {
            get => (Brush)GetValue(Color1Property);
            set => SetValue(Color1Property, value);
        }

        public Brush Color2
        {
            get => (Brush)GetValue(Color2Property);
            set => SetValue(Color2Property, value);
        }

        public Brush Color3
        {
            get => (Brush)GetValue(Color3Property);
            set => SetValue(Color3Property, value);
        }
        public Brush Color4
        {
            get => (Brush)GetValue(Color4Property);
            set => SetValue(Color4Property, value);
        }


        public long? Value1
        {
            get => (long?)GetValue(Value1Property);
            set => SetValue(Value1Property, value);
        }

        public long? Value2
        {
            get => (long?)GetValue(Value2Property);
            set => SetValue(Value2Property, value);
        }

        public long? Value3
        {
            get => (long?)GetValue(Value3Property);
            set => SetValue(Value3Property, value);
        }

        public long? Value4
        {
            get => (long?)GetValue(Value4Property);
            set => SetValue(Value4Property, value);
        }
    }
}
