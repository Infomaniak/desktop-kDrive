using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Collections.Generic;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class ErrorList : UserControl
    {
        public ErrorList()
        {
            InitializeComponent();
        }

        public string Header
        {
            get { return (string)GetValue(HeaderProperty); }
            set { SetValue(HeaderProperty, value); }
        }

        public ICollection<Error> Errors
        {
            get { return (ICollection<Error>)GetValue(ErrorsProperty); }
            set { SetValue(ErrorsProperty, value); }
        }

        public static readonly DependencyProperty HeaderProperty =
            DependencyProperty.Register(nameof(Header), typeof(string), typeof(ErrorList), new PropertyMetadata(false));

        public static readonly DependencyProperty ErrorsProperty =
            DependencyProperty.Register(nameof(Errors), typeof(ICollection<Error>), typeof(ErrorList), new PropertyMetadata(false));
    }
}
