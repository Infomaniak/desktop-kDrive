using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using Windows.System;


namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.QuotaExceeded }
    )]
    public sealed partial class QuotaExceededError : UserControl
    {
        private Error Error { get; init; }
        public QuotaExceededError(Error error)
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
                Logger.Log(Logger.Level.Error, "Sync is null on a node level error");
                return;
            }
            Uri changeOfferUri = App.Constants.Drive.ChangeOfferUri(Error.Sync.Drive.DriveId);
            await Launcher.LaunchUriAsync(changeOfferUri);
        }
    }
}