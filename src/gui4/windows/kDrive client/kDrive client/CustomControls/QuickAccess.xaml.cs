using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class QuickAccess : UserControl
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel => _viewModel;

        public QuickAccess()
        {
            InitializeComponent();
        }

        private async void TrashButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? trashUrl = ViewModel.SelectedSync?.Drive.GetWebTrashUri();
            if (trashUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"TrashButton_Click: Launching trash URL: {trashUrl}");
                await Windows.System.Launcher.LaunchUriAsync(trashUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get trash URL.");
            }
        }

        private async void FavoritesButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? favoritesUrl = ViewModel.SelectedSync?.Drive.GetWebFavoritesUri();
            if (favoritesUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"Launching favorites URL: {favoritesUrl}");
                await Windows.System.Launcher.LaunchUriAsync(favoritesUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get favorites URL.");
            }

        }

        private async void SharedButton_Click(object sender, RoutedEventArgs e)
        {
            Uri? sharedUrl = ViewModel.SelectedSync?.Drive.GetWebSharedUri();
            if (sharedUrl != null)
            {
                Logger.Log(Logger.Level.Debug, $"Launching shared URL: {sharedUrl}");
                await Windows.System.Launcher.LaunchUriAsync(sharedUrl);
            }
            else
            {
                Logger.Log(Logger.Level.Error, "No sync selected or unable to get shared URL.");
            }
        }
    }
}
