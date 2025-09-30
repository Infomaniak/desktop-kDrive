using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using Windows.UI;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncSelector : UserControl
    {
        public readonly AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel => _viewModel;

        private FlyoutBase? _lastOpenedSyncMenu = null;
        private int _hoveredItemIndex = -1;

        public SyncSelector()
        {
            this.InitializeComponent();
            UpdateSelectionMode();
            ViewModel.AllSyncs.ObserveCollectionChanges().Subscribe(_ => UpdateSelectionMode());
            ViewModel.AllDrives.ObserveCollectionChanges().Subscribe(_ => UpdateSelectionMode());
        }

        private void DriveListView_ItemClick(object sender, ItemClickEventArgs e)
        {

            if (e.ClickedItem is not Drive drive)
                return;

            if (drive.Syncs.Count == 1)
            {
                ViewModel.SelectedSync = drive.Syncs[0];
                DriveFlyout.Hide();
            }
            else
            {
                var container = DriveListView.ContainerFromItem(drive) as ListViewItem;
                if (container == null)
                {
                    Logger.Log(Logger.Level.Error, "container is null in DriveListView_ItemClick for multi-sync drive.");
                    return;
                }
                if (container is null)
                {
                    Logger.Log(Logger.Level.Error, "container is null in DriveListView_ItemClick for multi-sync drive.");
                    return;
                }
                _hoveredItemIndex = DriveListView.IndexFromContainer(container);

                // This is a small trick to keep the flyout open.
                // When the user hovers over the drive element, the flyout opens. 
                // However, if the user then clicks the element, the flyout loses focus and starts closing.
                // Calling _lastOpenedSyncMenu.ShowAt(container) directly at this point has no effect,
                // because the closing animation triggered by the lost focus will still complete and close it.
                // To work around this, we first call ShowAt() on another element to interrupt/cancel the closing,
                // then immediately call ShowAt() again on the intended container.
                var fe = container?.ContentTemplateRoot as FrameworkElement;
                if (fe != null)
                {
                    var menu = Flyout.GetAttachedFlyout(fe);
                    if (_lastOpenedSyncMenu == menu)
                    {
                        menu.ShowAt(container.ContentTemplateRoot as Grid);
                        menu.ShowAt(container);
                    }
                    else
                    {
                        if (_lastOpenedSyncMenu is not null)
                            _lastOpenedSyncMenu.Hide();
                        _lastOpenedSyncMenu = menu;
                        menu.ShowAt(container);
                    }

                }
            }
        }

        private void SyncListView_ItemClick(object sender, ItemClickEventArgs e)
        {
            if (e.ClickedItem is Sync sync)
            {
                ViewModel.SelectedSync = sync;
                // Find the parent flyout in the visual tree and close it
                DriveFlyout.Hide();
            }
        }

        private void SyncAutoSuggestBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {
            if (args.Reason != AutoSuggestionBoxTextChangeReason.UserInput)
                return;

            ObservableCollection<Sync> autoSuggest = new();
            Drive? drive = sender.Tag as Drive;
            if (drive is null)
            {
                Logger.Log(Logger.Level.Error, "Drive is null in SyncAutoSuggestBox_TextChanged.");
                return;
            }

            string lowered = sender.Text.ToLowerInvariant();
            foreach (var sync in drive.Syncs)
            {
                if (sync.LocalPath.ToLowerInvariant().Contains(lowered) ||
                    sync.RemotePath.ToLowerInvariant().Contains(lowered))
                {
                    autoSuggest.Add(sync);
                }
            }

            if (autoSuggest.Count == 0)
            {
                sender.Foreground = new SolidColorBrush(Colors.Red);
            }
            else
            {
                sender.Foreground = (Application.Current.Resources["TextFillColorPrimaryBrush"] as SolidColorBrush) ?? new SolidColorBrush(Colors.Black); // TODO: Replace with the error color from design system once implemented
            }

            sender.ItemsSource = autoSuggest;
        }

        private void SyncAutoSuggestBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
        {
            if (args.ChosenSuggestion is Sync sync)
            {
                ViewModel.SelectedSync = sync;
                DriveFlyout.Hide();
                sender.Text = string.Empty;
            }
        }

        private async void DriveItem_PointerEntered(object sender, PointerRoutedEventArgs e)
        {
            // open the sender flyout when hovering the control
            var listViewItem = sender as ListViewItem;
            var fe = listViewItem?.ContentTemplateRoot as FrameworkElement;
            int itemIndex = DriveListView.IndexFromContainer(listViewItem);
            _hoveredItemIndex = itemIndex;
            if (_lastOpenedSyncMenu != null)
            {
                // Wait to see if the pointer entered another item within 200ms
                await Task.Delay(200);
                if (_hoveredItemIndex != itemIndex)
                    return;
            }

            if (fe == null)
            {
                if (_lastOpenedSyncMenu != null)
                    _lastOpenedSyncMenu.Hide();
                _lastOpenedSyncMenu = null;
                return;
            }

            var menu = Flyout.GetAttachedFlyout(fe);
            if (menu is not null)
            {
                menu.ShowAt(listViewItem);
            }

            if (_lastOpenedSyncMenu != null && _lastOpenedSyncMenu != menu)
                _lastOpenedSyncMenu.Hide();
            _lastOpenedSyncMenu = menu;
        }

        private void DriveItem_PointerExited(object sender, PointerRoutedEventArgs e)
        {
            var listViewItem = sender as ListViewItem;
            int itemIndex = DriveListView.IndexFromContainer(listViewItem);
            if (itemIndex == _hoveredItemIndex)
                _hoveredItemIndex = -1;
        }

        private void DriveListView_ContainerContentChanging(ListViewBase sender, ContainerContentChangingEventArgs args)
        {
            args.ItemContainer.PointerEntered += DriveItem_PointerEntered;
            args.ItemContainer.PointerExited += DriveItem_PointerExited;
        }

        private void SyncFlyout_Opened(object sender, object e)
        {
            Flyout? flyout = (sender as Flyout);
            if (flyout is null)
            {
                Logger.Log(Logger.Level.Error, "flyout is null in SyncFlyout_Opened.");
                return;
            }

            Drive? drive = ((flyout?.Target as ListViewItem)?.ContentTemplateRoot as FrameworkElement)?.DataContext as Drive ?? null;
            if (drive is null)
            {
                Logger.Log(Logger.Level.Error, "drive is null in SyncFlyout_Opened.");
                return;
            }
            ListView? listView = (flyout?.Content as StackPanel)?.Children.OfType<ListView>().FirstOrDefault();
            AutoSuggestBox? suggestBox = (flyout?.Content as StackPanel)?.Children.OfType<AutoSuggestBox>().FirstOrDefault();

            if (drive.Syncs.Count > 6)
            {
                if (suggestBox is null)
                {
                    Logger.Log(Logger.Level.Error, "suggestBox is null in SyncFlyout_Opened.");
                    return;
                }
                suggestBox.Text = "";
                suggestBox.ItemsSource = drive.Syncs;
                suggestBox.IsSuggestionListOpen = true;
                suggestBox.Tag = drive;
                suggestBox.Visibility = Visibility.Visible;
                if (listView is not null)
                    listView.Visibility = Visibility.Collapsed;
            }
            else
            {
                if (listView is null)
                {
                    Logger.Log(Logger.Level.Error, "listView is null in SyncFlyout_Opened.");
                    return;
                }
                listView.Visibility = Visibility.Visible;
                if (suggestBox is not null)
                    suggestBox.Visibility = Visibility.Collapsed;
            }


        }

        public void UpdateSelectionMode()
        {
            if (ViewModel.AllSyncs.Count == 0)
            {
                NoSyncConfigured.Visibility = Visibility.Visible;

                MultiDriveSelector.Visibility = Visibility.Collapsed;
                SingleDriveMultiSyncSelector.Visibility = Visibility.Collapsed;
                SingleDriveSingleSyncSelector.Visibility = Visibility.Collapsed;
            }
            else if (ViewModel.AllSyncs.Count == 1)
            {
                NoSyncConfigured.Visibility = Visibility.Collapsed;

                MultiDriveSelector.Visibility = Visibility.Collapsed;
                SingleDriveMultiSyncSelector.Visibility = Visibility.Collapsed;
                SingleDriveSingleSyncSelector.Visibility = Visibility.Visible;
            }
            else if (ViewModel.AllDrives.Count > 1)
            {
                NoSyncConfigured.Visibility = Visibility.Collapsed;
                MultiDriveSelector.Visibility = Visibility.Visible;
                SingleDriveMultiSyncSelector.Visibility = Visibility.Collapsed;
                SingleDriveSingleSyncSelector.Visibility = Visibility.Collapsed;
            }
            else // Only one drive but multiple syncs
            {
                NoSyncConfigured.Visibility = Visibility.Collapsed;
                MultiDriveSelector.Visibility = Visibility.Collapsed;
                SingleDriveMultiSyncSelector.Visibility = Visibility.Visible;
                SingleDriveSingleSyncSelector.Visibility = Visibility.Collapsed;
            }
            // Reload the DataTemplateSelector to force it to re-evaluate the templates
            var templateSelector = DriveListView.ItemTemplateSelector;
            DriveListView.ItemTemplateSelector = null;
            DriveListView.ItemTemplateSelector = templateSelector;
        }

        private void DriveFlyout_Opened(object sender, object e)
        {
            _lastOpenedSyncMenu = null;
            _hoveredItemIndex = -1;
        }
    }
    public class DriveTemplateSelector : DataTemplateSelector
    {
        public DataTemplate SingleSyncDrive { get; set; } = new DataTemplate();
        public DataTemplate MultiSyncDrive { get; set; } = new DataTemplate();

        protected override DataTemplate SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is Drive drive)
            {
                return (drive.Syncs != null && drive.Syncs.Count > 1) ? MultiSyncDrive : SingleSyncDrive;
            }

            return base.SelectTemplateCore(item, container);
        }
    }
}

