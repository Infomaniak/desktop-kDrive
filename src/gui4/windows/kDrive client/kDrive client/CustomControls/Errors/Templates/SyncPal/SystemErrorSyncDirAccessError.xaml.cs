using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using System.IO;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.SyncDirAccessError }
    )]
    public sealed partial class SystemErrorSyncDirAccessError : UserControl
    {
        private Error Error { get; init; }
        public SystemErrorSyncDirAccessError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                Title = Localizer.Instance.GetString("systemErrorSyncDirAccessErrorTitle"),
                Content = Localizer.Instance.GetString("localFileAccessErrorDialogOpenParentFolder"),
                DefaultButton = ContentDialogButton.Primary,
                SecondaryButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonOpenParentFolder"),
            };
            dialog.Content = new LocalAccessErrorDialog(Error) { XamlRoot = xamlRoot };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                string? absolutPath = Path.GetDirectoryName(Error.Sync?.LocalPath ?? "") ?? Error.Sync?.LocalPath;
                if (string.IsNullOrEmpty(absolutPath))
                {
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                else
                {
                    await Utility.OpenFolderSecurely(absolutPath);
                }
            }
        }
    }
}