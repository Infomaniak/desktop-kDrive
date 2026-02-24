using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.InvalidSync },
        ExitCauses = new[] { ExitCause.SyncDirAccessError }
    )]
    public sealed partial class InvalidSyncSyncDirAccessError : UserControl
    {
        private Error Error { get; init; }
        public InvalidSyncSyncDirAccessError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
