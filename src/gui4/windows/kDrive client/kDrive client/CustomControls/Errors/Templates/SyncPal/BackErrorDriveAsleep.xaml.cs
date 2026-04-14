using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.DriveAsleep, ExitCause.DriveWakingUp }
    )]
    public sealed partial class BackErrorDriveAsleep : UserControl
    {
        private Error Error { get; init; }
        public BackErrorDriveAsleep(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
        private void UserControl_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Bindings.StopTracking();
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

            if (!await Windows.System.Launcher.LaunchUriAsync(Error.Sync.Drive.GetWebUri()))
            {
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
