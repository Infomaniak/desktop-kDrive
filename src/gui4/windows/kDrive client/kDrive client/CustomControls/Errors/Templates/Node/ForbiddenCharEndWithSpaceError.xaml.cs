using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.ForbiddenCharEndWithSpace }
    )]
    public sealed partial class ForbiddenCharEndWithSpaceError : UserControl
    {
        private Error Error { get; init; }
        public ForbiddenCharEndWithSpaceError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}