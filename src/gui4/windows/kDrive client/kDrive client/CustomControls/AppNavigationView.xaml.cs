using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class AppNavigationView : NavigationView
    {
        public AppModel ViewModel { get; } =
           App.ServiceProvider.GetRequiredService<AppModel>();

        public Frame Frame { get { return ContentFrame; } }
        private readonly Dictionary<string, List<Type>> _navigationItemToPage = new Dictionary<string, List<Type>>()
        {
            {
                "HomePage", new List<Type>() {
                typeof(Pages.HomePage),
                typeof(Pages.DriveAccessDeniedPage),
                typeof(Pages.LogginErrorPage),
                typeof(Pages.NotRenewErrorPage),
                typeof(Pages.MaintenanceErrorPage),
                typeof(Pages.AsleepErrorPage)
            }},
            {
                "ActivityPage", new List<Type>() {
                typeof(Pages.ActivityPage),
                typeof(Pages.Errors.ErrorPage),
                typeof(Pages.Errors.ConflictQuickResolvePage),
                typeof(Pages.Errors.ResolveManyConflictPage)
            }},
            {
                "SettingsPage", new List<Type>() {
                typeof(Pages.Settings.SettingsPage),
                typeof(Pages.Settings.DriveManagementPage),
                typeof(Pages.Settings.DriveAdvancedSyncsPage),
                typeof(Pages.Settings.TemplateExclusionPage)
            }},
            {
                "StoragePage", new List<Type>() {
                typeof(Pages.StoragePage)
            }}
        };

        public AppNavigationView()
        {
            InitializeComponent();
        }

        private void AppNavigationView_Loaded(object sender, RoutedEventArgs e)
        {
            Frame.Navigated += Frame_Navigated;

            // Add an infobadge to SettingsItem
            var infoBadge = new InfoBadge()
            {
                HorizontalAlignment = HorizontalAlignment.Right,
                VerticalAlignment = VerticalAlignment.Center,
            };

            // Bind the visibility of the infobadge to the HasAvailableUpdates property of the ViewModel
            infoBadge.SetBinding(InfoBadge.VisibilityProperty, new Binding()
            {
                Source = ViewModel.Settings.UpdateManager,
                Path = new PropertyPath(nameof(ViewModel.Settings.UpdateManager.AvailableUpdate)),
                Converter = new Converters.IsNullToBoolOrVisibilityConverter(),
                ConverterParameter = "Inverted=True",
                Mode = BindingMode.OneWay
            });

            NavigationViewItem? settingItem = SettingsItem as NavigationViewItem;
            if (settingItem is null)
            {
                Logger.Log(Logger.Level.Error, "SettingsItem is not a NavigationViewItem. Cannot add InfoBadge.");
                return;
            }

            settingItem.InfoBadge = infoBadge;

        }
        private void AppNavigationView_Unloaded(object sender, RoutedEventArgs e)
        {
            Frame.Navigated -= Frame_Navigated;
        }

        private void Frame_Navigated(object sender, Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
        {
            UpdateSelectedItem();
        }


        private void OnItemInvoked(NavigationView sender, NavigationViewItemInvokedEventArgs args)
        {
            GoToNavigationViewItemPage(args.InvokedItemContainer as NavigationViewItem);

        }

        private void GoToNavigationViewItemPage(NavigationViewItemBase? item)
        {
            if (item is not null)
            {
                if (ContentFrame.CurrentSourcePageType.Name == item?.Tag?.ToString())
                    return;

                if (item?.Tag?.ToString() == "Settings" && ContentFrame.CurrentSourcePageType.Name == "SettingsPage")
                    return;
                // Navigate to the selected page
                if (_navigationItemToPage.TryGetValue(item?.Tag?.ToString() ?? "", out List<Type>? pageTypes))
                    ContentFrame.Navigate(pageTypes.FirstOrDefault());
                else
                    ContentFrame.Navigate(typeof(SettingsPage));
            }
        }

        private bool GetSyncSelectorIsEnabled(Sync? SelectedSync, object currentContent)
        {
            if (SelectedSync is null)
                return false;

            if (_navigationItemToPage["SettingsPage"].Contains(currentContent.GetType()))
                return false;

            return true;
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
            }
        }

        private async void NavigationViewItem_Tapped(object sender, TappedRoutedEventArgs e)
        {
            await Utility.OpenFolderSecurely(ViewModel.SelectedSync?.LocalPath ?? "");
        }
    }
}
