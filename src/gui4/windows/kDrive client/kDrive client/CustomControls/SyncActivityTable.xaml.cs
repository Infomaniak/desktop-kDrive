using DynamicData;
using DynamicData.Binding;
using KDrive.ViewModels;
using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDrive.CustomControls
{
    public sealed partial class SyncActivityTable : UserControl
    {
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel => _viewModel;
        private ReadOnlyObservableCollection<SyncActivity> _outGoingActivities = new(new());

        public SyncActivityTable()
        {
            InitializeComponent();
        }

        public bool? ShowIncomingActivity
        {
            get => (bool)GetValue(ShowIncomingActivityProperty);
            set => SetValue(ShowIncomingActivityProperty, value);
        }

        public static readonly DependencyProperty ShowIncomingActivityProperty =
            DependencyProperty.Register(
                nameof(ShowIncomingActivity),
                typeof(bool?),
                typeof(SyncActivityTable),
                new PropertyMetadata(true, OnShowIncomingActivityChanged));

        private static void OnShowIncomingActivityChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SyncActivityTable control)
            {
                control.RefreshFilteredActivities();
            }
        }

        private void RefreshFilteredActivities()
        {
            if (ActivityList == null) return;

            if (ShowIncomingActivity == null || ShowIncomingActivity == true)
            {
                ActivityList.ItemsSource = ViewModel.SelectedSync?.SyncActivities;
            }
            else
            {
                ViewModel.SelectedSync?
                    .SyncActivities
                    .ToObservableChangeSet()
                    .AutoRefresh()
                    .Filter(a => a.Direction != SyncActivityDirection.Incoming)
                    .Bind(out _outGoingActivities) // gives ReadOnlyObservableCollection<SyncActivity>
                    .Subscribe(); // keep subscription alive

                ActivityList.ItemsSource = _outGoingActivities;
            }
        }
        private void ListView_ContainerContentChanging(ListViewBase sender, ContainerContentChangingEventArgs args)
        {
            if (args.ItemContainer is ListViewItem item)
            {
                int index = sender.IndexFromContainer(item);

                if (item.ContentTemplateRoot is Grid itemGrid)
                {
                    itemGrid.Background = (index % 2 == 0)
                    ? new SolidColorBrush(Colors.White)
                    : new SolidColorBrush(Colors.WhiteSmoke);
                }
            }
        }

        private async void SyncActivityParent_Click(object sender, RoutedEventArgs e)
        {
            if (sender is HyperlinkButton btn && btn.DataContext is SyncActivity activity)
            {
                string? parentDir = Path.GetDirectoryName(activity.ParentFolderPath);
                if (Directory.Exists(parentDir))
                {
                    btn.IsEnabled = false;
                    Process.Start("explorer.exe", parentDir);
                    await Task.Delay(5000); // As the explorer might take some time to open avoid multiple clicks
                    btn.IsEnabled = true;
                }
            }
        }
    }
}

