using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.Pages.Popup
{
    public sealed partial class LogUploadPopup : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public CheckBox LastSessionCheckBox => CheckBox;
        public LogUploadPopup()
        {
            Logger.Log(Logger.Level.Info, "Navigated to LogUploadPopup - Initializing LogUploadPopup components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "LogUploadPopup components initialized");
        }
        private void Page_Unloaded(object sender, RoutedEventArgs e)
        {
            Bindings.StopTracking();
        }
    }
}
