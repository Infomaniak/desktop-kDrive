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
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Upsell
{
    public sealed partial class MyKsuiteInfoBar : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        public static readonly DependencyProperty DriveProperty =
           DependencyProperty.Register(
               nameof(Drive),
               typeof(Drive),
               typeof(MyKsuiteInfoBar),
               new PropertyMetadata(null));

        public Drive? Drive
        {
            get => (Drive?)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        public MyKsuiteInfoBar()
        {
            InitializeComponent();
        }

        private async void Button_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.Upsell, Analytics.Keys.EventName.myKsuitePlus);

            if (Drive is null)
            {
                Logger.LogError("Drive is null in ksuiteInfoBar, cannot open upsell link");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            var uriStore = App.ServiceProvider.GetRequiredService<IUriStore>();
            if (!await uriStore.OpenInBrowserAsync(uriStore.DriveUpsellUrl(Drive)))
            {
                Logger.LogWarning("Failed to open DriveUpsellUrl in browser");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }
    }
}
