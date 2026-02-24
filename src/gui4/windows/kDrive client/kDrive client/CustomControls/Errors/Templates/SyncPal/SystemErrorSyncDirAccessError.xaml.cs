using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.SyncDirAccessError }
    )]
    public sealed partial class SystemErrorSyncDirAccessError : UserControl
    {
        private Error Error { get; init; }
        public SystemErrorSyncDirAccessError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}