using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using Windows.System;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls.Errors
{
    public partial class RemoteDiskFullBar : UserControl
    {
        private AppModel ViewModel { get; } = App.ServiceProvider.GetRequiredService<AppModel>();

        public RemoteDiskFullBar()
        {
            InitializeComponent();
        }

        public Drive? Drive
        {
            get => (Drive?)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }

        public static readonly DependencyProperty DriveProperty =
            DependencyProperty.Register(nameof(Drive), typeof(Drive), typeof(RemoteDiskFullBar), new PropertyMetadata(null));

        private async void InfoBarButton_Click(object sender, RoutedEventArgs e)
        {
            Uri changeOfferUri = App.Constants.Drive.ChangeOfferUri(Drive?.DriveId);
            await Launcher.LaunchUriAsync(changeOfferUri);
        }

        private void InfoBar_Closed(InfoBar sender, InfoBarClosedEventArgs args)
        {
            if (Drive is not null)
                Drive.DisplayRemoteSpaceWarning = false;
        }
    }
}
