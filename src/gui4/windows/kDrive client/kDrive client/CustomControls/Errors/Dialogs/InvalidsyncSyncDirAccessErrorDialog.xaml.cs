using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors;

public partial class InvalidSyncSyncDirAccessErrorDialog : Page
{
    private Error Error { get; init; }
    public string FilePath => Error.Path ?? string.Empty;
    public Sync? Sync => Error.Sync;

    public InvalidSyncSyncDirAccessErrorDialog(Error error)
    {
        Error = error;
        InitializeComponent();
    }
}
