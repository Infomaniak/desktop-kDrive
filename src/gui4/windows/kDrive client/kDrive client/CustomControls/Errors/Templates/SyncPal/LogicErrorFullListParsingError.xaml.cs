using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.LogicError },
        ExitCauses = new[] { ExitCause.FullListParsingError }
    )]
    public sealed partial class LogicErrorFullListParsingError : UserControl
    {
        private Error Error { get; init; }
        public LogicErrorFullListParsingError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
