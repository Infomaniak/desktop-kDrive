using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.DriveMaintenance }
    )]
    public sealed partial class BackErrorDriveMaintenance : UserControl
    {
        private Error Error { get; init; }
        public BackErrorDriveMaintenance(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void OnRefreshClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (Error?.Sync is null)
            {
                Logger.Log(Logger.Level.Warning, "Refresh clicked but Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;

            if (!await Error.Sync.Start())
            {
                Utility.ShowUnexpectedErrorTeachingTip();
            }

            if (control is not null)
                control.IsEnabled = true;
        }
    }
}
