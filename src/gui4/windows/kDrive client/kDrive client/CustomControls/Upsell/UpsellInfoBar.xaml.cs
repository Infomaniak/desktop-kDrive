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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.ComponentModel;

namespace Infomaniak.kDrive.CustomControls.Upsell
{
    public sealed partial class UpsellInfoBar : UserControl
    {
        public static readonly DependencyProperty DriveProperty =
            DependencyProperty.Register(
                nameof(Drive),
                typeof(Drive),
                typeof(UpsellInfoBar),
                new PropertyMetadata(null, OnDriveChanged));

        public Drive? Drive
        {
            get => (Drive?)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        private bool CanShowUpsell =>
           (Drive?.IsAdmin ?? false) && (Drive?.PackIsFree ?? false);

        private bool MyKsuiteInfoBarLoad =>
           CanShowUpsell && (Drive?.PackUpsellTarget == UpsellTarget.MykSuiteToMykSuitePlus || Drive?.PackUpsellTarget == UpsellTarget.StandAloneToMykSuitePlus);

        private bool KsuiteInfoBarLoad =>
           CanShowUpsell && (Drive?.PackUpsellTarget == UpsellTarget.kSuiteUpgrade || Drive?.PackUpsellTarget == UpsellTarget.StandAloneTokSuitePro);

        public UpsellInfoBar()
        {
            InitializeComponent();
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            if (Drive is not null)
            {
                Drive.PropertyChanged -= OnDrivePropertyChanged;
                Drive.PropertyChanged += OnDrivePropertyChanged;
            }
            Bindings.Update();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            if (Drive is not null)
                Drive.PropertyChanged -= OnDrivePropertyChanged;
        }

        private static void OnDriveChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var control = d as UpsellInfoBar;

            if (control is null || !control.IsLoaded)
                return;

            if (e.OldValue is Drive oldDrive)
                oldDrive.PropertyChanged -= control.OnDrivePropertyChanged;

            if (e.NewValue is Drive newDrive)
                newDrive.PropertyChanged += control.OnDrivePropertyChanged;

            control.Bindings.Update();
        }

        private void OnDrivePropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(Drive.PackUpsellTarget) || e.PropertyName == nameof(Drive.PackIsFree) || e.PropertyName == nameof(Drive.IsAdmin))
                Bindings.Update();
        }
    }
}