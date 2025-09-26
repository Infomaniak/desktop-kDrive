using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Animation;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using System.Windows.Automation.Peers;
using System.Windows.Controls.Primitives;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class DoubleComboBox : UserControl
    {
        public readonly AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel { get { return _viewModel; } }

        private bool _pointerIsOverDriveSelectionPopUp = false;
        private const double _itemSize = 41;
        private const double _windowMargin = 10; // Minimum distance to keep between the window border and the DriveSelectionPopup
        public ObservableCollection<Sync> SyncAutoSuggestSuitable { get; set; } = new ObservableCollection<Sync>();

        public DoubleComboBox()
        {
            InitializeComponent();
        }

        private void DriveButton_Click(object sender, RoutedEventArgs e)
        {
            // open the Popup if it isn't open already
            if (!DriveSelectionPopup.IsOpen)
            {

                int selectedIndex = 0;
                ReadOnlyObservableCollection<Drive> drives = DriveListView.ItemsSource as ReadOnlyObservableCollection<Drive>;
                if (ViewModel?.SelectedSync?.Drive != null)
                    selectedIndex = drives?.IndexOf(ViewModel.SelectedSync.Drive) ?? 0;
                DriveSelectionPopup.IsOpen = true;
                GeneralTransform transform = DriveButton.TransformToVisual(null);
                double windowHeight = (App.Current as App)?.CurrentWindow?.Bounds.Height ?? 800;
                DriveListView.MaxHeight = windowHeight - 3 * _windowMargin;

                Windows.Foundation.Point position = transform.TransformPoint(new Windows.Foundation.Point(0, 0));
                double distanceToTop = position.Y;
                double distanceToBottom = windowHeight - position.Y;

                double calculatedDownSideOffset = (drives?.Count - selectedIndex) * _itemSize ?? 0;
                double calculatedUpSideOffset = selectedIndex * _itemSize;
                double calculatedTotalHeight = Math.Min(calculatedDownSideOffset + calculatedUpSideOffset, DriveListView.MaxHeight);

                if (distanceToBottom >= calculatedDownSideOffset && distanceToTop >= calculatedUpSideOffset)
                {
                    DriveSelectionPopup.VerticalOffset = -calculatedUpSideOffset;
                }
                else if (distanceToTop < calculatedUpSideOffset)
                {
                    DriveSelectionPopup.VerticalOffset = -distanceToTop + _windowMargin; // Stick to the top
                }
                else if (distanceToBottom < calculatedDownSideOffset)
                {
                    DriveSelectionPopup.VerticalOffset = -calculatedTotalHeight + distanceToBottom - 2 * _windowMargin; // Stick to the bottom
                }
                DriveListView.SelectedItem = ViewModel?.SelectedSync?.Drive;


                if (ViewModel?.SelectedSync?.Drive?.Syncs.Count > 1)
                {
                    SyncListView.ItemsSource = ViewModel.SelectedSync.Drive.Syncs;
                    SyncListView.SelectedItem = ViewModel.SelectedSync;
                }
            }
        }

        private void DriveButton_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            DriveSelectionPopup.HorizontalOffset = DriveButton.ActualOffset.X;
            DriveSelectionBorder.MinWidth = e.NewSize.Width;
        }

        private void DriveListView_ItemClick(object sender, ItemClickEventArgs e)
        {
            if (e.ClickedItem is Drive selectedDrive)
            {
                if (selectedDrive.Syncs.Count == 1)
                {
                    ViewModel.SelectedSync = selectedDrive.Syncs[0];
                    DriveSelectionPopup.IsOpen = false;
                    SyncSelectionPopup.IsOpen = false;
                }
                else
                {
                    int selectedIndex = (DriveListView.ItemsSource as ReadOnlyObservableCollection<Drive>)?.IndexOf(selectedDrive) ?? 0;
                    SyncSelectionPopup.VerticalOffset = DriveSelectionPopup.VerticalOffset + selectedIndex * _itemSize + 6;
                    OpenSyncSelection(selectedDrive.Syncs);
                }
            }
        }

        private void SyncListView_ItemClick(object sender, ItemClickEventArgs e)
        {
            if (e.ClickedItem is Sync selectedSync)
            {
                ViewModel.SelectedSync = selectedSync;
                DriveSelectionPopup.IsOpen = false;
                SyncSelectionPopup.IsOpen = false;
            }
        }

        private async void SyncSelectionPopup_Closed(object sender, object e)
        {
            await Task.Delay(200);
            if (!_pointerIsOverDriveSelectionPopUp)
            {
                DriveSelectionPopup.IsOpen = false;
            }
        }

        private async void OpenSyncSelection(ObservableCollection<Sync> syncs)
        {
            SyncSelectionPopup.HorizontalOffset = DriveSelectionPopup.HorizontalOffset + DriveSelectionBorder.ActualWidth - 10;
            if (syncs.Count > 6)
            {
                SyncAutoSuggestBox.Visibility = Visibility.Visible;
                SyncListView.Visibility = Visibility.Collapsed;
                SyncAutoSuggestBox.Text = " ";
                SyncAutoSuggestBox.Text = "";
                SyncAutoSuggestBox.Focus(FocusState.Programmatic);
                SyncSelectionPopup.IsOpen = true;
                SyncAutoSuggestSuitable.Clear();
                foreach(var sync in syncs)
                {
                    SyncAutoSuggestSuitable.Add(sync);
                }
                await Task.Delay(100);
                SyncAutoSuggestBox.IsSuggestionListOpen = true;
            }
            else if (syncs.Count > 1)
            {
                SyncListView.Visibility = Visibility.Visible;
                SyncAutoSuggestBox.Visibility = Visibility.Collapsed;
                SyncListView.ItemsSource = syncs;
                SyncSelectionPopup.IsOpen = true;
            }
            else
            {
                SyncSelectionPopup.IsOpen = false;
            }
        }

        private void DriveListView_PointerEntered(object sender, PointerRoutedEventArgs e)
        {
            _pointerIsOverDriveSelectionPopUp = true;
        }

        private async void DriveListView_PointerExited(object sender, PointerRoutedEventArgs e)
        {
            _pointerIsOverDriveSelectionPopUp = false;
            if (!SyncSelectionPopup.IsOpen)
            {
                await Task.Delay(500);
                if (!SyncSelectionPopup.IsOpen && !_pointerIsOverDriveSelectionPopUp)
                    DriveSelectionPopup.IsOpen = false;
            }
        }

        private void SyncAutoSuggestBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {
            // Since selecting an item will also change the text,
            // only listen to changes caused by user entering text.
            if (args.Reason == AutoSuggestionBoxTextChangeReason.UserInput || args.Reason == AutoSuggestionBoxTextChangeReason.ProgrammaticChange)
            {
                var loweredText = sender.Text.ToLower();
                Drive? selectedDrive = DriveListView.SelectedItem as Drive;
                if (selectedDrive == null || selectedDrive.Syncs.Count == 0)
                {
                    sender.ItemsSource = null;
                    Logger.Log(Logger.Level.Warning, "No drive selected or selected drive has no syncs.");
                    return;
                }
                SyncAutoSuggestSuitable.Clear();
                ObservableCollection<Sync> syncs = selectedDrive.Syncs[0].Drive.Syncs;
                foreach (var sync in syncs)
                {
                    if (sync.LocalPath.ToLower().Contains(loweredText) || sync.RemotePath.ToLower().Contains(loweredText))
                        SyncAutoSuggestSuitable.Add(sync);
                }
                if (SyncAutoSuggestSuitable.Count == 0)
                {
                    sender.Foreground = new SolidColorBrush(Windows.UI.Color.FromArgb(255, 200, 0, 0));
                }
                else
                {
                    sender.Foreground = null;
                }
            }
        }

        private void SyncAutoSuggestBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
        {
            SyncSelectionPopup.IsOpen = false;
            ViewModel.SelectedSync = args.ChosenSuggestion as Sync;
            SyncAutoSuggestBox.Text = "";
        }

        private void Popup_Opened(object sender, object e)
        {

            var childBorder = (Border?)(sender as Microsoft.UI.Xaml.Controls.Primitives.Popup)?.Child;
            if (childBorder == null)
            {
                Logger.Log(Logger.Level.Error, "Popup opened but child border is null.");
                return;
            }
            childBorder.Child.Focus(FocusState.Pointer);
            var sb = new Microsoft.UI.Xaml.Media.Animation.Storyboard();

            // Scale animation
            var scaleY = new Microsoft.UI.Xaml.Media.Animation.DoubleAnimation
            {
                From = 0.8,
                To = 1,
                Duration = TimeSpan.FromMilliseconds(100),
                EasingFunction = new Microsoft.UI.Xaml.Media.Animation.CubicEase { EasingMode = Microsoft.UI.Xaml.Media.Animation.EasingMode.EaseOut }
            };

            Storyboard.SetTarget(scaleY, childBorder.RenderTransform);
            Storyboard.SetTargetProperty(scaleY, "ScaleY");

            sb.Children.Add(scaleY);

            // Opacity fade
            var fade = new Microsoft.UI.Xaml.Media.Animation.DoubleAnimation
            {
                From = 0.5,
                To = 1,
                Duration = TimeSpan.FromMilliseconds(50)
            };
            Storyboard.SetTarget(fade, childBorder);
            Storyboard.SetTargetProperty(fade, "Opacity");
            sb.Children.Add(fade);
            sb.Begin();

        }

        private void DriveSelectionPopup_Closed(object sender, object e)
        {
        }
    }

    public class DriveTemplateSelector : DataTemplateSelector
    {
        public DataTemplate SingleSyncDrive { get; set; }
        public DataTemplate MultiSyncDrive { get; set; }

        protected override DataTemplate SelectTemplateCore(object item, DependencyObject container)
        {
            if (item is Drive drive)
            {
                if (drive.Syncs != null && drive.Syncs.Count > 1)
                    return MultiSyncDrive;

                return SingleSyncDrive;
            }

            return base.SelectTemplateCore(item, container);
        }
    }
}

