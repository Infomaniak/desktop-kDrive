using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class ForbiddenCharError : UserControl
    {
        private Error _error;
        public ForbiddenCharError(Error error)
        {
            this.InitializeComponent();
            _error = error;
        }
    }
}
