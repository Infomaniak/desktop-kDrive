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
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
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
            _errorPageVM = new ErrorPageVM();
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            if (_errorPageVM is not null)
                _errorPageVM.Dispose();
        }

        private void BreadcrumbActivity_Click(object sender, object e)
        {
            Logger.Log(Logger.Level.Debug, "Navigating to ActivityPage");
            Frame.Navigate(typeof(ActivityPage));
        }

        private void OnSelectedSyncChanged(object sender, SelectedSyncChangedEventArgs e)
        {
            if (e.NewValue is not null)
            {
                if (_errorPageVM is not null)
                    _errorPageVM.Sync = e.NewValue;
                else
                    _errorPageVM = new ErrorPageVM { Sync = e.NewValue };
            }
        }

        private void ManyConflicts_ActionClick(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Frame.Navigate(typeof(ConflictQuickResolvePage));
        }
    }
}