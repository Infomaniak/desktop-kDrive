using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.UnableToStartVfs }
    )]
    public sealed partial class SystemErrorUnableToStartVfs : UserControl
    {
        private Error Error { get; init; }
        public SystemErrorUnableToStartVfs(Error error)
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
                Title = Localizer.Instance.GetString("errDialogSystemUnableToStartVfsTitle"),
                Content = Localizer.Instance.GetString("errDialogSystemUnableToStartVfsDescription"),
                DefaultButton = ContentDialogButton.Close,
                CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonSynchronizeOffline")
            };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                if (Error.Sync is null)
                {
                    Logger.Log(Logger.Level.Error, "Error.Sync is null");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                if (!await Error.Sync.ChangeSyncType(Types.SyncType.Offline))
                {
                    ContentDialog errorDialog = new ContentDialog
                    {
                        XamlRoot = xamlRoot,
                        Title = Localizer.Instance.GetString("dialogSyncModeChangeErrorTitle"),
                        CloseButtonText = Localizer.Instance.GetString("buttonCancel"),
                        Content = Localizer.Instance.GetString("dialogSyncModeChangeErrorContent")
                    };
                    await errorDialog.ShowAsync();
                }
            }
        }
    }
}
