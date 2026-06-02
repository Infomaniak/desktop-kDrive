using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        CancelTypes = new[] { CancelType.FileRescued }
    )]
    public sealed partial class FileRescuedError : UserControl
    {
        private Error Error { get; init; }
        public FileRescuedError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}