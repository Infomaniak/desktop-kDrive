using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors.Templates.Node
{
    [ErrorMetadata(
        Levels = new[] { ErrorLevel.Node },
        NodeTypes = new[] { NodeType.File, NodeType.Directory },
        ExitCodes = new[] { ExitCode.SystemError },
        ExitCauses = new[] { ExitCause.NotEnoughDiskSpace }
    // CancelTypes = new[] { CancelType.None },
    // ConflictTypes = new[] { ConflictType.None },
    // ExitCodes = new[] { ExitCode.Unknown },
    // ExitCauses = new[] { ExitCause.Unknown }
    )]
    public sealed partial class NotEnoughDiskSpaceError : UserControl
    {
        private Error Error { get; init; }
        public NotEnoughDiskSpaceError(Error error)
        {
            this.InitializeComponent();
            Error = error;
        }
    }
}