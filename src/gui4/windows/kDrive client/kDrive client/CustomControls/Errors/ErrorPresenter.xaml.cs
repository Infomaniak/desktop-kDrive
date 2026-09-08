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

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class ErrorPresenter : UserControl
    {
        public ErrorPresenter()
        {
            this.InitializeComponent();
        }

        public Error Error
        {
            get
            {
                return (Error)GetValue(ErrorProperty);
            }
            set
            {
                SetValue(ErrorProperty, value);
                UpdateContent();
            }
        }

        public static readonly DependencyProperty ErrorProperty =
         DependencyProperty.Register(nameof(Error), typeof(Error), typeof(ErrorCard), new PropertyMetadata(null));

        private void UpdateContent()
        {
            if (Error is null)
            {
                Content = null;
                Logger.Log(Logger.Level.Error, "ErrorSelector: Error is null, clearing content.");
                return;
            }

            UserControl errorControl = ErrorFactory.CreateErrorControl(Error);
            //errorControl.XamlRoot = this.XamlRoot;
            Content = errorControl;
        }
    }
}
