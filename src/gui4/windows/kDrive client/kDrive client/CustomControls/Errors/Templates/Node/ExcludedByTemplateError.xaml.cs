using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        CancelTypes = new[] { CancelType.ExcludedByTemplate }
    )]
    public sealed partial class ExcludedByTemplateError : UserControl
    {
        private Error Error { get; init; }
        public ExcludedByTemplateError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            var frame = ((Application.Current as App)?.CurrentWindow as MainWindow)?.AppNavView.Frame;
            if (frame is null)
            {
                Logger.Log(Logger.Level.Warning, "Unable to fetch main frame");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }
            frame.Navigate(typeof(Pages.Settings.TemplateExclusionPage));
        }
    }
}