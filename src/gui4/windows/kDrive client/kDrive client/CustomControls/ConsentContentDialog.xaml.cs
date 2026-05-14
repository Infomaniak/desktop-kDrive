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
using System;
using System.Threading.Tasks;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.Window;

namespace Infomaniak.kDrive.CustomControls
{
    public enum ConsentResult
    {
        Allowed,
        Refused,
        Cancelled
    }

    public sealed partial class ConsentDialog : UserControl
    {
        public ConsentDialog()
        {
            this.InitializeComponent();
        }


        // Dependency properties
        public string LogoUriStr
        {
            get => (string)GetValue(LogoUriStrProperty);
            set => SetValue(LogoUriStrProperty, value);
        }

        public static readonly DependencyProperty LogoUriStrProperty =
            DependencyProperty.Register(nameof(LogoUriStr), typeof(string), typeof(ConsentDialog), new PropertyMetadata(default(string)));

        public string Description
        {
            get => (string)GetValue(DescriptionProperty);
            set => SetValue(DescriptionProperty, value);
        }

        public static readonly DependencyProperty DescriptionProperty =
            DependencyProperty.Register(nameof(Description), typeof(string), typeof(ConsentDialog), new PropertyMetadata(default(string)));

        public bool CurrentValue
        {
            get => (bool)GetValue(CurrentValueProperty);
            set => SetValue(CurrentValueProperty, value);
        }

        public static readonly DependencyProperty CurrentValueProperty =
            DependencyProperty.Register(nameof(CurrentValue), typeof(bool), typeof(ConsentDialog), new PropertyMetadata(false));

        /// <summary>
        /// Shows the dialog and returns the user's decision.
        /// </summary>
        public async Task<ConsentResult> ShowAsync(XamlRoot xamlRoot)
        {
            Dialog.XamlRoot = xamlRoot;
            Toggle.IsOn = CurrentValue;
            var result = await Dialog.ShowAsync();

            if (result == ContentDialogResult.Primary)
                return Toggle.IsOn ? ConsentResult.Allowed : ConsentResult.Refused;

            return ConsentResult.Cancelled;
        }
    }
}
