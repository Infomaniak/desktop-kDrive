using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.ApplicationModel.DataTransfer;

namespace Infomaniak.kDrive.CustomControls.Errors;

public partial class ConflictDialog : Page
{
    private Error Error { get; init; }
    public string FilePath => Error.Path ?? string.Empty;
    public Sync? Sync => Error.Sync;

    public ConflictDialog(Error error)
    {
        Error = error;
        InitializeComponent();
    }
}
