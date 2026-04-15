using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        InconsistencyTypes = new[] { InconsistencyType.ForbiddenCharOnlySpaces }
    )]
    public sealed partial class ForbiddenCharOnlySpacesError : UserControl
    {
        private Error Error { get; init; }
        public ForbiddenCharOnlySpacesError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }

        private async void ErrorCard_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            if (!await Error.OpenItemInWebViewAsync())
                Utility.ShowUnexpectedErrorTeachingTip();
        }
    }
}