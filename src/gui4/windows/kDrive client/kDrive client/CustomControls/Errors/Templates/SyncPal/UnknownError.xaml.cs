using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.SyncPal
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.SyncPal },
        ExitCodes = new[] { ExitCode.Unknown }
    )]
    public sealed partial class UnknownError : UserControl
    {
        private Error Error { get; init; }
        public UnknownError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}
