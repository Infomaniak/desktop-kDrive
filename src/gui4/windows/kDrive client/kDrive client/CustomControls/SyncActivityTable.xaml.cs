using DynamicData;
using DynamicData.Binding;
using KDrive.Types;
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
using System.Reactive.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Foundation;
using Windows.Foundation.Collections;
using WinRT;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace KDrive.CustomControls
{
    public sealed partial class SyncActivityTable : UserControl
    {
        internal AppModel _viewModel = ((App)Application.Current).Data;
        internal AppModel ViewModel => _viewModel;
        private ObservableCollection<SyncActivity> _outGoingActivities = new();
        private Int64 _insertionCounter = 0;
        public SyncActivityTable()
        {
            InitializeComponent();
            ViewModel.SelectedSync?
                    .SyncActivities.ToObservableChangeSet().OnItemAdded(a => { if (a.Direction == SyncDirection.Incoming) _outGoingActivities.Insert(0, a); }).Subscribe();
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
                ActivityList.ItemsSource = _outGoingActivities;
            }
        }

        private async void SyncActivityParent_Click(object sender, RoutedEventArgs e)
        {
            if (sender is HyperlinkButton btn && btn.DataContext is SyncActivity activity)
            {
                if (Directory.Exists(activity.ParentFolderPath))
                {
                    btn.IsEnabled = false;
                    Utility.OpenFolderSecurely(activity.ParentFolderPath);
                    await Task.Delay(5000); // As the explorer might take some time to open avoid multiple clicks
                    btn.IsEnabled = true;
                }
            }
            else
            {
                Logger.Log(Logger.Level.Warning, $"Unexpected call from {sender.ToString()}");
            }
        }

        private void Grid_Loading(FrameworkElement sender, object args)
        {
            if (sender is Grid grid)
            {
                grid.Background = (_insertionCounter++ % 2 == 0)
                        ? new SolidColorBrush(Colors.Transparent)
                        : new SolidColorBrush(Colors.WhiteSmoke);
            }

        }
    }
}

