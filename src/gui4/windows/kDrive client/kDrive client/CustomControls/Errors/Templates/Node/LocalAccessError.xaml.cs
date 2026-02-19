using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        //InconsistencyTypes = new[] { InconsistencyType.None }
        // CancelTypes = new[] { CancelType.None },
        // ConflictTypes = new[] { ConflictType.None },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.FileAccessError }
    )]
    public sealed partial class LocalAccessError : UserControl
    {
        private Error _error;
        public LocalAccessError(Error error)
        {
            this.InitializeComponent();
            _error = error;
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                Title = Localizer.Instance.GetString("localFileAccessErrorTitle"),
                Content = Localizer.Instance.GetString("localFileAccessErrorDialogOpenParentFolder"),
                DefaultButton = ContentDialogButton.Primary,
                SecondaryButtonText = Localizer.Instance.GetString("buttonClose"),
                PrimaryButtonText = Localizer.Instance.GetString("localFileAccessErrorDialogOpenParentFolder"),
            };
            dialog.Content = new LocalAccessErrorDialog(_error) { XamlRoot = xamlRoot };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                string absolutPath = Path.Combine(_error.Sync?.LocalPath ?? "", _error.Path);
                if (string.IsNullOrEmpty(Path.GetDirectoryName(absolutPath)))
                {
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                else
                {
                    await Utility.OpenFolderSecurely(absolutPath);
                }
            }
        }
    }
}
