using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
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

        private const double _popupSacing = 4;
        public DoubleComboBox()
        {
            InitializeComponent();
        }

        // Handles the Click event on the Button on the page and opens the Popup.
        private void DriveButton_Click(object sender, RoutedEventArgs e)
        {
            // open the Popup if it isn't open already
            if (!DriveSelectionPopup.IsOpen)
            {
                DriveSelectionPopup.IsOpen = true;
                DriveListView.SelectedItem = ViewModel?.SelectedSync?.Drive;
                if (ViewModel?.SelectedSync?.Drive?.Syncs.Count > 1)
                {
                    SyncListView.ItemsSource = ViewModel.SelectedSync.Drive.Syncs;
                    SyncSelectionPopup.IsOpen = true;
                    SyncListView.SelectedItem = ViewModel.SelectedSync;
                }
            }
        }

        private void DriveButton_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            DriveSelectionPopup.HorizontalOffset = DriveButton.ActualOffset.X;
            DriveSelectionBorder.MinWidth = e.NewSize.Width;
            SyncSelectionPopup.HorizontalOffset = DriveSelectionPopup.HorizontalOffset + DriveSelectionPopup.ActualWidth - 20;
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
                    SyncListView.ItemsSource = selectedDrive.Syncs;
                    SyncSelectionPopup.IsOpen = true;
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

        private void SyncSelectionPopup_Closed(object sender, object e)
        {
            if (DriveSelectionPopup.FocusState != FocusState.Unfocused)
            {
             //   DriveSelectionPopup.IsOpen = false;
            }
        }
    }
}
