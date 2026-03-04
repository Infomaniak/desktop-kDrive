using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.NameLength }
    )]
    public sealed partial class NameLengthError : UserControl
    {
        private Error Error { get; init; }
        public NameLengthError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}