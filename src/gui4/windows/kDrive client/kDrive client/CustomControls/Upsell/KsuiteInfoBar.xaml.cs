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
    public sealed partial class KsuiteInfoBar : UserControl
    {
        public static readonly DependencyProperty DriveProperty =
           DependencyProperty.Register(
               nameof(Drive),
               typeof(Drive),
               typeof(KsuiteInfoBar),
               new PropertyMetadata(null));

        public Drive? Drive
        {
            get => (Drive?)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        public KsuiteInfoBar()
        {
            InitializeComponent();
            Loaded += KsuiteBanner_Loaded;
        }

        private void KsuiteBanner_Loaded(object sender, RoutedEventArgs e) => UpdateGradientCanvasOpacity();

        private void GradientCanvas_ActualThemeChanged(Microsoft.UI.Xaml.FrameworkElement sender, object args) => UpdateGradientCanvasOpacity();

        private void UpdateGradientCanvasOpacity() => GradientCanvas.Opacity = (GradientCanvas.ActualTheme == ElementTheme.Light) ? 0.8 : 0.3;

        private void GradientCanvas_SizeChanged(object sender, Microsoft.UI.Xaml.SizeChangedEventArgs e)
        {
            const double ratio = 0.95;
            Ellipse1.Width = e.NewSize.Width * ratio;
            Ellipse2.Width = e.NewSize.Width * ratio;
            Ellipse3.Width = e.NewSize.Width * ratio;
        }

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            _analyticsService.TrackClick(Analytics.Keys.Category.Upsell, Analytics.Keys.EventName.kSuitePro);

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
