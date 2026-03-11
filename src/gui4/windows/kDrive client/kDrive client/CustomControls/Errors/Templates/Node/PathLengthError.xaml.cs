using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.PathLength }
    )]
    public sealed partial class PathLengthError : UserControl
    {
        private Error Error { get; init; }
        public PathLengthError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}