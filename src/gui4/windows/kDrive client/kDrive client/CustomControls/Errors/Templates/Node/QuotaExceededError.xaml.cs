using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.BackError },
        ExitCauses = new[] { ExitCause.QuotaExceeded }
    )]
    public sealed partial class QuotaExceededError : UserControl
    {
        private Error Error { get; init; }
        public QuotaExceededError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}