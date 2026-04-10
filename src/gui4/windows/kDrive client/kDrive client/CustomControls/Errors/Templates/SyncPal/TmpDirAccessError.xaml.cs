using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.SystemError},
        ExitCauses = new[] { ExitCause.TmpDirAccessError }
    )]
    public sealed partial class TmpDirAccessError : UserControl
    {
        private Error Error { get; init; }
        public TmpDirAccessError(Error error)
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
            App.ExitApplicationAndShutdownServer();
        }
    }
}