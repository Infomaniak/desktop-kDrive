using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors;

public partial class SystemErrorSyncDirChangedErrorDialog : Page
{
    private Error Error { get; init; }
    public string FilePath => Error.Path ?? string.Empty;
    public Sync? Sync => Error.Sync;

    public SystemErrorSyncDirChangedErrorDialog(Error error)
    {
        Error = error;
        InitializeComponent();
    }
    private void Page_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        Bindings.StopTracking();
    }
}
