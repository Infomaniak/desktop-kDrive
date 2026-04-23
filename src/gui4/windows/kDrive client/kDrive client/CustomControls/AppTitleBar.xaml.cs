using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
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
        }

        private Visibility TitleBarContentVisibility(Sync? selectedSync, bool isUpdateRequired)
        {
            if (isUpdateRequired)
                return Visibility.Collapsed;
            if (selectedSync is null)
                return Visibility.Collapsed;

            return Visibility.Visible;
        }
    }
}
