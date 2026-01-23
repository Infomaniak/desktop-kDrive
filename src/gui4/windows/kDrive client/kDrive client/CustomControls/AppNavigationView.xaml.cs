using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using System;
using System.Collections.Generic;
using System.Linq;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class AppNavigationView : NavigationView
    {
        public AppModel ViewModel { get; } =
           App.ServiceProvider.GetRequiredService<AppModel>();

        public Frame Frame { get { return ContentFrame; } }
        private Dictionary<string, List<Type>> _navigationItemToPage = new Dictionary<string, List<Type>>()
        {
            { "HomePage", new List<Type>() {
                typeof(Pages.HomePage),
                typeof(Pages.DriveAccessDeniedPage),
                typeof(Pages.LoggingErrorPage),
                typeof(Pages.NotRenewErrorPage),
                typeof(Pages.MaintenanceErrorPage),
                typeof(Pages.AsleepErrorPage)
            } },
            { "ActivityPage", new List<Type>() { typeof(Pages.ActivityPage), typeof(Pages.ErrorPage) } },
            { "SettingsPage", new List<Type>() { typeof(Pages.Settings.SettingsPage), typeof(Pages.Settings.DriveManagementPage) } },
            { "StoragePage", new List<Type>() { typeof(Pages.StoragePage) } }
        };

        public AppNavigationView()
        {
            InitializeComponent();
            Loaded += AppNavigationView_Loaded;
        }

        private void AppNavigationView_Loaded(object sender, RoutedEventArgs e)
        {
            Frame.Navigated += Frame_Navigated;
        }

        private void Frame_Navigated(object sender, Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
        {
            UpdateSelectedItem();
        }

        private void OnSelectionChanged(NavigationView sender, NavigationViewSelectionChangedEventArgs args)
        {
            GoToNavigationViewItemPage(args.SelectedItem as NavigationViewItem);
        }

        private void OnItemInvoked(NavigationView sender, NavigationViewItemInvokedEventArgs args)
        {
            GoToNavigationViewItemPage(args.InvokedItem as NavigationViewItem);

        }

        private void GoToNavigationViewItemPage(NavigationViewItem? item)
        {
            if (item is not null)
            {
                // Navigate to the selected page
                if (_navigationItemToPage.TryGetValue(item?.Tag?.ToString() ?? "", out List<Type>? pageTypes))
                {
                    ContentFrame.Navigate(pageTypes.FirstOrDefault());
                    return;
                }

                Logger.Log(Logger.Level.Info, $"Unknown navigation tag: {item.Tag}... Going to HomePage");
                ContentFrame.Navigate(typeof(SettingsPage));
            }
        }

        private void UpdateSelectedItem()
        {
            var newSelectedItem = MenuItems.OfType<NavigationViewItem>().FirstOrDefault(item =>
                _navigationItemToPage.TryGetValue(item.Tag.ToString() ?? "", out List<Type>? pageTypes) &&
                pageTypes.Contains(ContentFrame.Content.GetType()));
            if (newSelectedItem is null)
                newSelectedItem = SettingsItem as NavigationViewItem;
            SelectedItem = newSelectedItem;
        }
        private void OnBackRequested(NavigationView sender, NavigationViewBackRequestedEventArgs args)
        {
            if (ContentFrame.CanGoBack)
            {
                ContentFrame.GoBack();
                SelectedItem = MenuItems.OfType<NavigationViewItem>().FirstOrDefault(item => item.Tag.ToString() == ((Frame)ContentFrame).Content.GetType().Name);
            }
        }

        private void NavigationViewItem_Tapped(object sender, TappedRoutedEventArgs e)
        {
            Utility.OpenFolderSecurely(ViewModel.SelectedSync?.LocalPath ?? "");
        }
    }
}
