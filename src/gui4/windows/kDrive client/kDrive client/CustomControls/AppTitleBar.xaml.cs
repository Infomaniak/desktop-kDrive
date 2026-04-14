using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class AppTitleBar : TitleBar
    {
        public AppModel ViewModel { get; } =
           App.ServiceProvider.GetRequiredService<AppModel>();

        public AppTitleBar()
        {
            InitializeComponent();
            Unloaded += AppTitleBar_Unloaded;
        }

        private void AppTitleBar_Unloaded(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }
    }
}
