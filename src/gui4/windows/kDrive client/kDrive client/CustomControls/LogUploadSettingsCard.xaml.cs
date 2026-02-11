using CommunityToolkit.WinUI.Controls;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class LogUploadSettingsCard : SettingsCard
    {
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
            ContentDialog dialog = new ContentDialog();
            dialog.XamlRoot = XamlRoot;
            dialog.Title = Localizer.Localizer.Instance.GetString("logUploadPopupTitle");
            dialog.DefaultButton = ContentDialogButton.Primary;
            dialog.PrimaryButtonText = Localizer.Localizer.Instance.GetString("send");
            dialog.SecondaryButtonText = Localizer.Localizer.Instance.GetString("cancel");
            var popupPage = new Pages.Popup.LogUploadPopup();
            dialog.Content = popupPage;

            var result = await dialog.ShowAsync();

            if (result == ContentDialogResult.Primary)
                await LogUploadManager.StartUpload(!(popupPage.LastSessionCheckBox.IsChecked ?? false));
            else
                Logger.Log(Logger.Level.Info, "Log upload canceled");
        }

        private async void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            await LogUploadManager.CancelUpload();
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
