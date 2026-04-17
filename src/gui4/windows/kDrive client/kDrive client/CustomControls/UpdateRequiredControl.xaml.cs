using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class UpdateRequiredControl : UserControl
    {
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();
        public UpdateRequiredControl()
        {
            this.InitializeComponent();
        }
                private Visibility UpdateButtonVisibility(bool fetchingUpdate, AppVersion? availableUpdate)
        {
            if (fetchingUpdate)
                return Visibility.Visible;

            if (availableUpdate is not null)
                return Visibility.Visible;

            return Visibility.Collapsed;
        }

        private Visibility ManualUpdateButtonVisibility(bool fetchingUpdate, AppVersion? availableUpdate)
        {
            return (UpdateButtonVisibility(fetchingUpdate, availableUpdate) == Visibility.Visible) ? Visibility.Collapsed : Visibility.Visible;
        }

        private async void UpdateButton_Click(object sender, RoutedEventArgs e)
        {
            Button? button = sender as Button;
            if (button is not null)
                button.IsEnabled = false;

            if (!await UpdateManager.StartUpdate())
            {
                Logger.Log(Logger.Level.Error, "Update process failed to start.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            else
            {
                await Task.Delay(5000); // Prevent from multiple click as the installer can take some time to appear
            }

            if (button is not null)
                button.IsEnabled = true;
        }

        private async void ManualUpdateButton_Click(object sender, RoutedEventArgs e)
        {
            if (!await Localizer.Instance.TryLaunchUriAsync("downloadAppUrl"))
                Utility.ShowUnexpectedErrorTeachingTip();
        }
    }
}
