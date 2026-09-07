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
using System;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LicensesHyperlink : StackPanel
    {
        public LicensesHyperlink()
        {
            InitializeComponent();
        }

        public AppVersion? DisplayedVersion
        {
            get => (AppVersion?)GetValue(DisplayedVersionProperty);
            set => SetValue(DisplayedVersionProperty, value);

        }

        public static readonly DependencyProperty DisplayedVersionProperty =
         DependencyProperty.Register(
             nameof(DisplayedVersion),
             typeof(AppVersion),
             typeof(UpdateExpander),
             new PropertyMetadata(null, OnDisplayedVersionChanged));

        private static void OnDisplayedVersionChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is not LicensesHyperlink control)
            {
                Logger.Log(Logger.Level.Warning, "DependencyObject is not of type LicensesHyperlink, this is unexpected.");
                return;
            }
            control.Refresh();
        }

        public void Refresh()
        {
            if (DisplayedVersion is null)
            {
                Logger.Log(Logger.Level.Warning, "DisplayedVersion is null, this is unexpected.");
                return;
            }
            ExpandedTextBox.Text = Localizer.Instance.GetString("aboutAppVersionCopyright", DisplayedVersion.Tag, DisplayedVersion.BuildVersion, DateTime.Now.Year);
        }
    }
}
