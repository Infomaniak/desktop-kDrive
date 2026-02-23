using Infomaniak.kDrive.CustomControls;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;
using System;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    public sealed partial class ErrorPage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        private ErrorPageVM? _errorPageVM;
        public ErrorPage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ErrorPage - Initializing ErrorPage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ErrorPage components initialized");
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            ViewModel.SelectedSyncChanged += OnSelectedSyncChanged;
            if (ViewModel.SelectedSync is not null)
            {
                _errorPageVM = new ErrorPageVM(ViewModel.SelectedSync);
            }
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            ViewModel.SelectedSyncChanged -= OnSelectedSyncChanged;
        }

        private void BreadcrumbActivity_Click(object sender, object e)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to ActivityPage");
            Frame.Navigate(typeof(ActivityPage));
        }

        private void OnSelectedSyncChanged(object sender, SelectedSyncChangedEventArgs e)
        {
            _errorPageVM = null;
            if (e.NewValue is not null)
            {
                _errorPageVM = new ErrorPageVM(e.NewValue);
            }
        }
    }
}