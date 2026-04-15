using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.NetworkError },
        ExitCauses = new[] { ExitCause.Unknown, ExitCause.SocketsDefuncted, ExitCause.NetworkTimeout }
    )]
    public sealed partial class NetworkErrorOther : UserControl
    {
        private Error Error { get; init; }
        public NetworkErrorOther(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
