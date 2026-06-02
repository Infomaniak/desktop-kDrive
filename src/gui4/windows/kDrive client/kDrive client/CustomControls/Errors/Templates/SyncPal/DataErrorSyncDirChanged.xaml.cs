using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.DataError },
        ExitCauses = new[] { ExitCause.SyncDirChanged },
        NodeTypes = new[] { NodeType.File, NodeType.Directory, NodeType.Unknown }
    )]
    public sealed partial class DataErrorSyncDirChanged : UserControl
    {
        private Error Error { get; init; }
        public DataErrorSyncDirChanged(Error error)
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
                Title = Localizer.Instance.GetString("dialogDataErrorSyncDirChangedTitle"),
                DefaultButton = ContentDialogButton.Primary,
                SecondaryButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("buttonCreateNewSync"),
                Content = new SystemErrorSyncDirChangedErrorDialog(Error) { XamlRoot = xamlRoot }
            };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                var frame = ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;
                if (frame is null)
                {
                    Logger.Log(Logger.Level.Error, "Failed to navigate to the sync setup page after a sync directory change error because the main frame could not be found.");
                    return;
                }

                var destPage = (Error.Sync?.IsAdvanced ?? false) ? typeof(Pages.Settings.DriveAdvancedSyncsPage) : typeof(Pages.Settings.DriveManagementPage);
                frame.Navigate(destPage, Error.Sync?.Drive);
            }
        }
    }
}