using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public sealed partial class ErrorPresenter : UserControl
    {
        public ErrorPresenter()
        {
            this.InitializeComponent();
        }

        public Error Error
        {
            get
            {
                return (Error)GetValue(ErrorProperty);
            }
            set
            {
                SetValue(ErrorProperty, value);
                UpdateContent();
            }
        }

        public static readonly DependencyProperty ErrorProperty =
         DependencyProperty.Register(nameof(Error), typeof(Error), typeof(ErrorCard), new PropertyMetadata(null));

        private void UpdateContent()
        {
            if (Error is null)
            {
                Content = null;
                Logger.Log(Logger.Level.Error, "ErrorSelector: Error is null, clearing content.");
                return;
            }

            UserControl errorControl = ErrorFactory.CreateErrorControl(Error);
            //errorControl.XamlRoot = this.XamlRoot;
            Content = errorControl;
        }
    }
}
