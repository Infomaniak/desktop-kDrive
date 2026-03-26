using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

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

        private void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            var frame = ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;

            if (frame is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to navigate to the subscription page after a quota exceeded error because the main frame could not be found.");
                return;
            }

            frame.Navigate(typeof(Pages.StoragePage));
        }
    }
}