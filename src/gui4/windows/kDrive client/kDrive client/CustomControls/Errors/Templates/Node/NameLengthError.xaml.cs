using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.NameLength }
    )]
    public sealed partial class NameLengthError : UserControl
    {
        private Error Error { get; init; }
        public NameLengthError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (!string.IsNullOrEmpty(Error.RemoteNodeId) && !await Error.OpenItemInWebViewAsync())
                Utility.ShowUnexpectedErrorTeachingTip();
            else if (!string.IsNullOrEmpty(Error.Path) && !await Utility.OpenFolderSecurely(System.IO.Path.Combine(Error.Sync.LocalPath, Error.Path)))
                Utility.ShowUnexpectedErrorTeachingTip();
            else if (string.IsNullOrEmpty(Error.RemoteNodeId) && string.IsNullOrEmpty(Error.Path))
            {
                Logger.Log(Logger.Level.Warning, $"Error {Error.Id} has no RemoteNodeId or Path, cannot open in web or file explorer.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }

        }
    }
}