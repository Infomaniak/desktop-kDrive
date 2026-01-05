using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class ErrorBar : UserControl
    {
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public ErrorBar()
        {
            InitializeComponent();
        }

        public static readonly DependencyProperty FrameProperty =
            DependencyProperty.Register(nameof(Frame), typeof(Frame), typeof(ErrorBar), new PropertyMetadata(null));

        private void InfoBarButton_Click(object sender, RoutedEventArgs e)
        {
            Frame? frame = Utility.GetFrame(this);
            if (frame is not null)
            {
                Logger.Log(Logger.Level.Info, "Navigating to ErrorPage.");
                frame.Navigate(typeof(Pages.ErrorPage));
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Could not find Frame in visual tree to navigate to error page");
            }
        }
    }
}
