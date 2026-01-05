using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using System;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class ErrorPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public ErrorPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ErrorPage - Initializing ErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ErrorPage components initialized");
        }
    }
}
