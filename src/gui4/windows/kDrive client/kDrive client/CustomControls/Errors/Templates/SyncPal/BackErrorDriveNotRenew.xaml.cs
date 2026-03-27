using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.DriveNotRenew }
    )]
    public sealed partial class BackErrorDriveNotRenew : UserControl
    {
        private Error Error { get; init; }
        private string DescriptionKey => (Error.Sync?.Drive.IsAdmin ?? false) ? "driveLockedAdminErrorDescription" : "driveLockedErrorDescription";
        private string ActionTextKey => (Error.Sync?.Drive.IsAdmin ?? false) ? "buttonUpdateSubscription" : "buttonRefresh";
        public BackErrorDriveNotRenew(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (Error.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Error.Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            if (Error.Sync.Drive.IsAdmin)
            {
                if (!await Windows.System.Launcher.LaunchUriAsync(App.Constants.Drive.RenewUrl(Error.Sync.Drive.DriveId)))
                    Utility.ShowUnexpectedErrorTeachingTip();
            }
            else
            {
                if (!await Error.Sync.Start())
                    Utility.ShowUnexpectedErrorTeachingTip();
            }

            if (!await Windows.System.Launcher.LaunchUriAsync(Error.Sync.Drive.GetWebUri()))
            {
                Utility.ShowUnexpectedErrorTeachingTip();
            }

            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
