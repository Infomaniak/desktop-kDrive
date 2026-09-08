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

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class ErrorBar : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public ErrorBar()
        {
            InitializeComponent();
        }

        public static readonly DependencyProperty FrameProperty =
            DependencyProperty.Register(nameof(Frame), typeof(Frame), typeof(ErrorBar), new PropertyMetadata(null));

        private void InfoBarButton_Click(object sender, RoutedEventArgs e)
        {
            Frame? frame = Utility.GetFrame(this);
            if (frame is not null)
            {
                Logger.Log(Logger.Level.Info, "Navigating to ErrorPage.");
                _analyticsService.TrackClick(Analytics.Keys.Category.ActivityPage, Analytics.Keys.EventName.OpenErrors);
                frame.Navigate(typeof(Pages.Errors.ErrorPage));
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Could not find Frame in visual tree to navigate to error page");
            }
        }
    }
}
