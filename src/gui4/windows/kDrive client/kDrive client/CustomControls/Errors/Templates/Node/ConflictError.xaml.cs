using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ConflictTypes = new[] { ConflictType.CreateCreate, ConflictType.EditEdit }
    )]
    public sealed partial class ConflictError : UserControl
    {
        private Error Error { get; init; }
        public ConflictError(Error error)
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
            ContentDialog dialog = new ContentDialog
            {
                XamlRoot = xamlRoot,
                DefaultButton = ContentDialogButton.Secondary,
                PrimaryButtonText = Localizer.Instance.GetString("buttonClose"),
            };

            List<Error> allConflict = Error.Sync.SyncErrors.Where(e => (e.ConflictType == ConflictType.EditEdit || e.ConflictType == ConflictType.CreateCreate)).ToList();
            // Set the content first to allow the dialog to measure properly
            dialog.Content = new ConflictDialog(allConflict, dialog) { XamlRoot = xamlRoot };

            // Apply the style to allow wider content
            dialog.Resources["ContentDialogMaxWidth"] = 800d;
            dialog.Resources["ContentDialogMaxHeight"] = 800d;

            if (await dialog.ShowAsync() == ContentDialogResult.Primary)
            {
                string absolutPath = Path.Combine(Error.Sync?.LocalPath ?? "", Error.Path);
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