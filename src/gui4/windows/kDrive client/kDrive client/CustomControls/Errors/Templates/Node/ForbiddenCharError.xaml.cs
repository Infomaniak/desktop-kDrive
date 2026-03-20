using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;
using System;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.ForbiddenChar, InconsistencyType.NotYetSupportedChar }
    )]
    public sealed partial class ForbiddenCharError : UserControl
    {
        private Error Error { get; init; }
        public ForbiddenCharError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (Error.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Error Sync is null. Cannot navigate to node.");
                return;
            }

            if (!string.IsNullOrEmpty(Error.RemoteNodeId))
            {
                try
                {
                    if (!await Launcher.LaunchUriAsync(App.Constants.Drive.itemUri(Error.Sync.Drive.DriveId, Error.RemoteNodeId)))
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to launch URI for node with RemoteNodeId: {Error.RemoteNodeId}");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to launch URI for node with RemoteNodeId: {Error.RemoteNodeId}. Exception: {ex.Message}");
                }

            }
            else if (!string.IsNullOrEmpty(Error.LocalNodeId))
            {
                string localPath = System.IO.Path.Combine(Error.Sync.LocalPath, Error.Path);
                if (!await Utility.OpenFolderSecurely(localPath))
                {
                    Logger.Log(Logger.Level.Error, $"Failed to open folder at path: {localPath}");
                }
            }

        }
    }
}
