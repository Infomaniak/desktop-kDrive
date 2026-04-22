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

        private Frame? Frame => ((App.Current as App)?.CurrentWindow as MainWindow)?.AppNavView?.Frame ?? null;

        public AppTitleBar()
        {
            InitializeComponent();
        }

        private Visibility TitleBarContentVisibility(bool isModelInitialized, Sync? selectedSync, bool isUpdateRequired)
        {
            if (!isModelInitialized || isUpdateRequired)
                return Visibility.Collapsed;
            if (selectedSync is null)
                return Visibility.Collapsed;

            return Visibility.Visible;
        }

        private void TitleBar_BackRequested(TitleBar sender, object args)
        {
            if (Frame is not null && Frame.CanGoBack)
                Frame?.GoBack();
        }
    }
}
