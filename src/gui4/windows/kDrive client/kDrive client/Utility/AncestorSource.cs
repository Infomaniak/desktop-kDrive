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
using Microsoft.UI.Xaml.Media;
using System;

namespace Infomaniak.kDrive
{
    public static class AncestorSource
    {
        public static readonly DependencyProperty AncestorTypeProperty =
            DependencyProperty.RegisterAttached(
                "AncestorType",
                typeof(Type),
                typeof(AncestorSource),
                new PropertyMetadata(default(Type), OnAncestorTypeChanged)
        );

        public static void SetAncestorType(FrameworkElement element, Type value) =>
            element.SetValue(AncestorTypeProperty, value);

        public static Type GetAncestorType(FrameworkElement element) =>
            (Type)element.GetValue(AncestorTypeProperty);

        private static void OnAncestorTypeChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            FrameworkElement target = (FrameworkElement)d;
            if (target.IsLoaded)
                SetDataContext(target);
            else
                target.Loaded += OnTargetLoaded;
        }

        private static void OnTargetLoaded(object sender, RoutedEventArgs e)
        {
            FrameworkElement target = (FrameworkElement)sender;
            target.Loaded -= OnTargetLoaded;
            SetDataContext(target);
        }

        private static void SetDataContext(FrameworkElement target)
        {
            Type ancestorType = GetAncestorType(target);
            if (ancestorType != null)
                target.DataContext = FindParent(target, ancestorType);
        }

        private static object? FindParent(DependencyObject dependencyObject, Type ancestorType)
        {
            DependencyObject parent = VisualTreeHelper.GetParent(dependencyObject);
            if (parent is null)
                return null;

            if (ancestorType.IsAssignableFrom(parent.GetType()))
                return parent;

            return FindParent(parent, ancestorType);
        }
    }
}
