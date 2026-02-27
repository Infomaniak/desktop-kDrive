using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        CancelTypes = new[] { CancelType.Edit }
    )]
    public sealed partial class EditCancelError : UserControl
    {
        private Error Error { get; init; }
        public EditCancelError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}