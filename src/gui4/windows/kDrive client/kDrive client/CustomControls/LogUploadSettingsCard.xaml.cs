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
using CommunityToolkit.WinUI.Controls;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LogUploadSettingsCard : SettingsCard
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();

        public LogUploadSettingsCard()
        {
            InitializeComponent();
        }
        public LogUploadManager? LogUploadManager
        {
            get { return (LogUploadManager?)GetValue(LogUploadManagerProperty); }
            set { SetValue(LogUploadManagerProperty, value); }
        }

        public static readonly DependencyProperty LogUploadManagerProperty =
           DependencyProperty.Register(nameof(LogUploadManager), typeof(LogUploadManager), typeof(LogUploadSettingsCard), new PropertyMetadata(null));

        private async void SendLogsButton_Click(object sender, RoutedEventArgs e)
        {
            if (LogUploadManager is null)
            {
                Logger.Log(Logger.Level.Error, "LogUploadManager is not set for LogUploadSettingsCard");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = XamlRoot,
                Title = Localizer.Instance.GetString("logUploadPopupTitle"),
                DefaultButton = ContentDialogButton.Primary,
                PrimaryButtonText = Localizer.Instance.GetString("buttonSend"),
                CloseButtonText = Localizer.Instance.GetString("buttonCancel")
            };
            var popupPage = new Pages.Popup.LogUploadPopup();
            dialog.Content = popupPage;

            var result = await dialog.ShowAsync();

            if (result == ContentDialogResult.Primary)
            {
                _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.SendLogToSupport);
                await LogUploadManager.StartUpload(!(popupPage.LastSessionCheckBox.IsChecked ?? false));
            }
            else
            {
                Logger.Log(Logger.Level.Info, "Log upload canceled");
            }
        }

        private async void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            await LogUploadManager.CancelUpload();
            _analyticsService.TrackClick(Analytics.Keys.Category.AdvancedSettingsPage, Analytics.Keys.EventName.CancelLogToSupport);
        }
    }

    // DataTemplateSelector that maps LogUploadState values to their corresponding UI templates.
    public class LogUploadStateTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? NoneTemplate { get; set; }
        public DataTemplate? ArchivingTemplate { get; set; }
        public DataTemplate? UploadingTemplate { get; set; }
        public DataTemplate? SuccessTemplate { get; set; }
        public DataTemplate? FailedTemplate { get; set; }
        public DataTemplate? CancelRequestedTemplate { get; set; }
        public DataTemplate? CanceledTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is LogUploadState state)
            {
                return state switch
                {
                    LogUploadState.None => NoneTemplate,
                    LogUploadState.Archiving => ArchivingTemplate,
                    LogUploadState.Uploading => UploadingTemplate,
                    LogUploadState.Success => SuccessTemplate,
                    LogUploadState.Failed => FailedTemplate,
                    LogUploadState.CancelRequested => CancelRequestedTemplate,
                    LogUploadState.Canceled => CanceledTemplate,
                    _ => NoneTemplate,
                };
            }
            return base.SelectTemplateCore(item);
        }
    }
}
