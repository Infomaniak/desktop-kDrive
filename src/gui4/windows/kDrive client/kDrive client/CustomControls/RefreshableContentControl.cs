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

    /* A custom WinUI 3 control that extends `ContentControl` while preserving its default behavior.
     * In addition to reacting to changes in `Content`, the control exposes a `RefreshSource` dependency property that can be bound using `{x:Bind}`.
     * Whenever `RefreshSource` changes, the control triggers the same content-update logic as a normal `ContentControl` when its `Content` changes, even if the `Content` instance itself remains unchanged.
     * This allows `ContentTemplateSelector` to reevaluate the appropriate template when a property affecting template selection changes, without modifying or replacing the underlying `Content`.
    */
    public sealed class RefreshableContentControl : ContentControl
    {
        public static readonly DependencyProperty RefreshSourceProperty =
            DependencyProperty.Register(
                nameof(RefreshSource),
                typeof(object),
                typeof(RefreshableContentControl),
                new PropertyMetadata(null, OnRefreshSourceChanged));

        public object? RefreshSource
        {
            get => GetValue(RefreshSourceProperty);
            set => SetValue(RefreshSourceProperty, value);
        }

        private static void OnRefreshSourceChanged(
            DependencyObject dependencyObject,
            DependencyPropertyChangedEventArgs args)
        {
            if (dependencyObject is not RefreshableContentControl control)
                return;

            // Reevaluate the content as if it had changed.
            // This also triggers ContentTemplateSelector without replacing Content.
            control.OnContentChanged(control.Content, control.Content);
        }
    }
}
