using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using static Infomaniak.kDrive.ViewModels.AppModel;

namespace Infomaniak.kDrive.Pages.Errors
{
    public sealed partial class ConflictQuickResolvePage : Page
    {
        private AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        private ErrorPageVM? _errorPageVM;
        public ConflictQuickResolvePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to ConflictQuickResolvePage - Initializing components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "ConflictQuickResolvePage components initialized");
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
        private void RadioButton_Loaded(object sender, RoutedEventArgs e)
        {
            if (sender is RadioButton radioButton)
            {
                var glyphGrid = FindChild<Grid>(radioButton, "OuterEllipse");
                if (glyphGrid is not null)
                    glyphGrid.VerticalAlignment = VerticalAlignment.Center;
            }
        }

        private static T? FindChild<T>(DependencyObject parent, string childNameInside) where T : FrameworkElement
        {
            for (var i = 0; i < VisualTreeHelper.GetChildrenCount(parent); i++)
            {
                var child = VisualTreeHelper.GetChild(parent, i);
                if (child is FrameworkElement fe && fe.Name == childNameInside)
                    return fe.Parent as T;

                var result = FindChild<T>(child, childNameInside);
                if (result is not null)
                    return result;
            }
            return null;
        }

        private async void ApplyButton_Click(object sender, RoutedEventArgs e)
        {
            ConflictResolutionStrategy resolutionStrategy;
            if (KeepMostRecentRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepMostRecent;
            }
            else if (KeepLocalRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepLocal;
            }
            else if (KeepRemoteRadioButton.IsChecked == true)
            {
                resolutionStrategy = ConflictResolutionStrategy.KeepRemote;
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Apply button clicked without a resolution strategy selected. No action will be taken.");
                return;
            }

            if (_errorPageVM is null)
            {
                Logger.Log(Logger.Level.Error, "_errorPageVM is null when Apply button clicked. This should never happen, but if it does, we log the error and re-enable the buttons to allow the user to try again or choose to manage conflicts individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            ApplyButton.IsEnabled = false;
            ManageIndividuallyButton.IsEnabled = false;

            if (_errorPageVM.Sync is null)
            {
                Logger.Log(Logger.Level.Error, "_errorPageVM.Sync is null when Apply button clicked. This should never happen, but if it does, we log the error and re-enable the buttons to allow the user to try again or choose to manage conflicts individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
                ApplyButton.IsEnabled = true;
                ManageIndividuallyButton.IsEnabled = true;
                return;
            }

            if (!await _errorPageVM.Sync.SolveConflictsQuick(resolutionStrategy))
            {
                Logger.Log(Logger.Level.Error, "Failed to resolve conflicts quickly. Re-enabling buttons to allow user to try again or manage individually.");
                Utility.ShowUnexpectedErrorTeachingTip();
            }
            else
            {
                Frame.Navigate(typeof(ActivityPage));
            }
            ApplyButton.IsEnabled = true;
            ManageIndividuallyButton.IsEnabled = true;
        }

        private void ManageIndividuallyButton_Click(object sender, RoutedEventArgs e)
        {
            Frame.Navigate(typeof(ResolveManyConflictPage));
        }
    }
}