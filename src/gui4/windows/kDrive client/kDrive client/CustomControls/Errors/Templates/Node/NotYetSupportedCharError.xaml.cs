using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.NotYetSupportedChar }
    )]
    public sealed partial class NotYetSupportedCharError : UserControl
    {
        private Error Error { get; init; }
        public NotYetSupportedCharError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}