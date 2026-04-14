using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.FileTooBig }
    )]
    public sealed partial class FileTooBigError : UserControl
    {
        private Error Error { get; init; }

        private string DescriptionLabel
        {
            get => (Error.Sync?.Drive?.IsAdmin ?? false) ? Localizer.Instance.GetString("errFileTooBigAdminDescription") : Localizer.Instance.GetString("errFileTooBigDescription");
        }
        public FileTooBigError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
        private void UserControl_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }
    }
}