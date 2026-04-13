using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.NotEnoughDiskSpace }
    )]
    public sealed partial class SystemErrorNotEnoughDiskSpace : UserControl
    {
        private Error Error { get; init; }
        public SystemErrorNotEnoughDiskSpace(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
        private void UserControl_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            bool result = await Windows.System.Launcher.LaunchUriAsync(new Uri("ms-settings:storagesense"));
            if (!result)
            {
                Logger.Log(Logger.Level.Warning, "Failed to launch settings for NotEnoughDiskSpaceError");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
        }
    }
}
