using Infomaniak.kDrive.Pages.Settings;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Navigation;
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
        private const int _maxStackSize = 5;

        public AppNavigationView()
        {
            InitializeComponent();
        }

        private void AppNavigationView_Loaded(object sender, RoutedEventArgs e)
        {
            Frame.Navigated += Frame_Navigated;
            Frame.Navigate(typeof(Pages.HomePage));

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

        private void Frame_Navigated(object sender, Microsoft.UI.Xaml.Navigation.NavigationEventArgs e)
        {
            UpdateSelectedItem();

            var frame = (Microsoft.UI.Xaml.Controls.Frame)sender;

            TrimStack(frame.BackStack, _maxStackSize);
            TrimStack(frame.ForwardStack, _maxStackSize);
        }

        private void TrimStack(IList<PageStackEntry> stack, int maxSize)
        {
            while (stack.Count > maxSize)
            {
                // Remove oldest entry (index 0 = bottom of stack)
                stack.RemoveAt(0);
            }
        }

        private void OnItemInvoked(NavigationView sender, NavigationViewItemInvokedEventArgs args)
        {
            GoToNavigationViewItemPage(args.InvokedItemContainer as NavigationViewItem);
        }

        private void GoToNavigationViewItemPage(NavigationViewItemBase? item)
        {
            if (item is not null)
            {
                // If the selected item is already displayed, do nothing
                if (ContentFrame.CurrentSourcePageType?.Name == item?.Tag?.ToString())
                    return;

                // We cannot set the tag of the SettingsItem because it is handled by the navigationView it self.
                if (item == SettingsItem as NavigationViewItemBase && ContentFrame.CurrentSourcePageType?.Name == "SettingsPage")
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

            if (currentContent is null)
                return false;

            if (_navigationItemToPage.TryGetValue("SettingsPage", out var settingsPages) && settingsPages.Contains(currentContent.GetType()))
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

        private async void NavigationViewItem_Tapped(object sender, TappedRoutedEventArgs e)
        {
            await Utility.OpenFolderSecurely(ViewModel.SelectedSync?.LocalPath ?? "");
        }

        private Visibility GetSyncSelectorVisibility(bool isPaneOpen, IList<Sync> syncs)
        {
            if (isPaneOpen && syncs.Count > 0)
                return Visibility.Visible;
            else
                return Visibility.Collapsed;
        }
    }
}
