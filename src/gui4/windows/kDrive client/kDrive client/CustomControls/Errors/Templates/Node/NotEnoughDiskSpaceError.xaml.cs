using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.NotEnoughDiskSpace }
    )]
    public sealed partial class NotEnoughDiskSpaceError : UserControl
    {
        private Error Error { get; init; }
        public NotEnoughDiskSpaceError(Error error)
        {
            this.InitializeComponent();
            Error = error;
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