using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.DataError },
        ExitCauses = new[] { ExitCause.SyncDirChanged }
    )]
    public sealed partial class DataErrorSyncDirChanged : UserControl
    {
        private Error Error { get; init; }
        public DataErrorSyncDirChanged(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
