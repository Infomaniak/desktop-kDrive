using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.IO;

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
        private Error Error { get; init; }
        public LocalAccessError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, RoutedEventArgs e)
        {
            var xamlRoot = this.XamlRoot;
            if (xamlRoot is null)
            {
                return;
            }

            if(Error.Sync is null) {          
                Logger.Log(Logger.Level.Error, "Error.Sync is null");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                Title = Localizer.Instance.GetStringCombine1s("errLocalFileAccessTitle", Error.NodeTypeTranslationKey),
                DefaultButton = ContentDialogButton.Primary,
                PrimaryButtonText = Localizer.Instance.GetString("buttonOpenParentFolder"),
                CloseButtonText = Localizer.Instance.GetString("buttonClose"),
            };
            dialog.Content = new LocalAccessErrorDialog(Error) { XamlRoot = xamlRoot };

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                string absolutPath = Path.Combine(Error.Sync.LocalPath, Error.Path);
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
