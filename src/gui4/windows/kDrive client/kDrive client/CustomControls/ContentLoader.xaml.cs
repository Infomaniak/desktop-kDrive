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
    public sealed partial class ContentLoader : UserControl
    {
        public ContentLoader()
        {
            InitializeComponent();
        }

        public bool IsLoading
        {
            get { return (bool)GetValue(IsLoadingProperty); }
            set { SetValue(IsLoadingProperty, value); }
        }
        public object LoaderContent
        {
            get { return GetValue(LoaderContentProperty); }
            set { SetValue(LoaderContentProperty, value); }
        }
        public double LoaderMinHeight
        {
            get { return (double)GetValue(MinLoaderHeightProperty); }
            set { SetValue(MinLoaderHeightProperty, value); }
        }

        public double LoaderMinWidth
        {
            get { return (double)GetValue(MinLoaderWidthProperty); }
            set { SetValue(MinLoaderWidthProperty, value); }
        }

        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(ContentLoader), new PropertyMetadata(false));

        public static readonly DependencyProperty LoaderContentProperty =
            DependencyProperty.Register(nameof(LoaderContent), typeof(object), typeof(ContentLoader), new PropertyMetadata(null));

        public static readonly DependencyProperty MinLoaderHeightProperty =
            DependencyProperty.Register(nameof(LoaderMinHeight), typeof(double), typeof(ContentLoader), new PropertyMetadata(0.0));

        public static readonly DependencyProperty MinLoaderWidthProperty =
            DependencyProperty.Register(nameof(LoaderMinWidth), typeof(double), typeof(ContentLoader), new PropertyMetadata(0.0));


    }
}
