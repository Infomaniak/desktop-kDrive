using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.ForbiddenChar }
    // CancelTypes = new[] { CancelType.None },
    // ConflictTypes = new[] { ConflictType.None },
    // ExitCodes = new[] { ExitCode.Unknown },
    // ExitCauses = new[] { ExitCause.Unknown }
    )]
    public sealed partial class ForbiddenCharError : UserControl
    {
        private Error Error { get; init; }
        public ForbiddenCharError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
