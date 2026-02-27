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
        ExitCauses = new[] { ExitCause.SyncDirDiskMissing },
        NodeTypes = new[] { NodeType.File, NodeType.Directory, NodeType.Unknown }
    )]
    public sealed partial class SystemErrorSyncDirDiskMissing : UserControl
    {
        private Error Error { get; init; }
        public SystemErrorSyncDirDiskMissing(Error error)
        {
            this.InitializeComponent();
            Error = error;

            error.Path = Error.Sync?.LocalPath ?? "";
            error.NodeType = Types.NodeType.Directory;
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
                Title = Localizer.Instance.GetString("systemErrorSyncDirMissingErrorTitle"),
                DefaultButton = ContentDialogButton.Primary,
                SecondaryButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonRestartSync"),
            };
            dialog.Content = new SystemErrorSyncDirDiskMissingErrorDialog(Error) { XamlRoot = xamlRoot };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                string? absolutPath = Path.GetDirectoryName(Error.Sync?.LocalPath ?? "") ?? Error.Sync?.LocalPath;
                if (string.IsNullOrEmpty(absolutPath))
                {
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                else
                {
                    if (Error.Sync is null)
                    {
                        Logger.Log(Logger.Level.Error, "Error.Sync is null in SystemErrorSyncDirDiskMissing. Cannot proceed with action click.");
                        Utility.ShowUnexpectedErrorTeachingTip();
                        return;
                    }

                    if (!await Error.Sync.Start())
                    {
                        Logger.Log(Logger.Level.Error, "Failed to restart sync in SystemErrorSyncDirDiskMissing.");
                        Utility.ShowTeachingTipFromKeys("systemErrorSyncDirMissingErrorTitle");
                    }
                }
            }
        }
    }
}