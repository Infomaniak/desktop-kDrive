using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        CancelTypes = new[] { CancelType.Create }
    )]
    public sealed partial class CreateCancelError : UserControl
    {
        private Error Error { get; init; }
        public CreateCancelError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}