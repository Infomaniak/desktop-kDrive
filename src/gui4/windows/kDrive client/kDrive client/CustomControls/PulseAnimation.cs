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
using Microsoft.UI.Xaml.Media.Animation;
using System;

namespace Infomaniak.kDrive.CustomControls
{
    // Attached property making an element fade in and out (30% <-> 90% opacity) while IsPulsing is true.
    public static class PulseAnimation
    {
        public static readonly DependencyProperty IsPulsingProperty =
            DependencyProperty.RegisterAttached("IsPulsing", typeof(bool), typeof(PulseAnimation), new PropertyMetadata(false, OnIsPulsingChanged));

        private static readonly DependencyProperty StoryboardProperty =
            DependencyProperty.RegisterAttached("Storyboard", typeof(Storyboard), typeof(PulseAnimation), new PropertyMetadata(null));

        public static bool GetIsPulsing(DependencyObject obj) => (bool)obj.GetValue(IsPulsingProperty);
        public static void SetIsPulsing(DependencyObject obj, bool value) => obj.SetValue(IsPulsingProperty, value);

        private static void OnIsPulsingChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is not UIElement element)
                return;

            var storyboard = (Storyboard?)element.GetValue(StoryboardProperty);
            if ((bool)e.NewValue)
            {
                if (storyboard is null)
                {
                    var animation = new DoubleAnimation
                    {
                        From = 0.8,
                        To = 0.5,
                        Duration = new Duration(TimeSpan.FromMilliseconds(800)),
                        AutoReverse = true,
                        RepeatBehavior = RepeatBehavior.Forever,
                        EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut }
                    };
                    Storyboard.SetTarget(animation, element);
                    Storyboard.SetTargetProperty(animation, nameof(UIElement.Opacity));
                    storyboard = new Storyboard();
                    storyboard.Children.Add(animation);
                    element.SetValue(StoryboardProperty, storyboard);
                }
                storyboard.Begin();
            }
            else if (storyboard is not null)
            {
                storyboard.Stop();
                element.Opacity = 1.0;
            }
        }
    }
}
