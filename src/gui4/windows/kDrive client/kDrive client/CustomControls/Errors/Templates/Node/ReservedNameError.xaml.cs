using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.ReservedName }
    )]
    public sealed partial class ReservedNameError : UserControl
    {
        private Error Error { get; init; }
        public ReservedNameError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}