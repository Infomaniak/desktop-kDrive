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

        private string DescriptionKey
        {
            get => (Error.Sync?.Drive?.IsAdmin ?? false) ? "errFileTooBigDescriptionAdmin" : "errFileTooBigDescription";
        }
        public FileTooBigError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}