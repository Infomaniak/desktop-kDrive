using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.InvalidSync },
        ExitCauses = new[] { ExitCause.SyncDirNestingError }
    )]
    public sealed partial class InvalidSyncSyncDirNestingError : UserControl
    {
        private Error Error { get; init; }
        public InvalidSyncSyncDirNestingError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
