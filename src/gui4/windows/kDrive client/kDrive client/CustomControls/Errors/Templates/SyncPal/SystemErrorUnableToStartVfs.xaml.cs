using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

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

        private async void OnOpenSyncSettingsClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (Error?.Sync is null)
            {
                Logger.Log(Logger.Level.Warning, "open sync settings clicked but Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            var frame = ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;
            if (frame is null)
            {
                Logger.Log(Logger.Level.Error, "Unable to navigate to SyncSettingsPage because Frame is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                if (control is not null)
                    control.IsEnabled = true;
                return;
            }
            var destPage = Error.Sync.IsAdvanced ? typeof(Pages.Settings.DriveAdvancedSyncsPage) : typeof(Pages.Settings.DriveManagementPage);
            (frame?.Navigate(destPage, Error.Sync.Drive));

            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
