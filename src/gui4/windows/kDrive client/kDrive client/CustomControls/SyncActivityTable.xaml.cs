using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
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
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reactive.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Data.Xml.Dom;
using Windows.Foundation;
using Windows.Foundation.Collections;
using WinRT;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncActivityTable : UserControl
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel => _viewModel;

        private readonly ObservableCollection<SyncActivity> _outGoingActivities = new();
        private IDisposable? _activitySubscription;

        public SyncActivityTable()
        {
            InitializeComponent();

            ViewModel.PropertyChanged += ViewModel_PropertyChanged;

            if (ViewModel.SelectedSync != null)
            {
                SubscribeToActivities(ViewModel.SelectedSync);
            }
            RefreshFilteredActivities();
        }

        ~SyncActivityTable()
        {
            _activitySubscription?.Dispose();
            ViewModel.PropertyChanged -= ViewModel_PropertyChanged;
        }

        private void ViewModel_PropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(ViewModel.SelectedSync) && ViewModel.SelectedSync is not null)
            {
                RefreshFilteredActivities();
            }
        }

        private void SubscribeToActivities(Sync sync)
        {
            _activitySubscription?.Dispose();
            _outGoingActivities.Clear();
            _activitySubscription = sync.SyncActivities
                .ToObservableChangeSet()
                .Filter(a => a.Direction == SyncDirection.Outgoing)
                .OnItemAdded(a => _outGoingActivities.Insert(0, a))
                .Subscribe();
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
            else
            {
                Logger.Log(Logger.Level.Warning, $"DependencyObject is not of type {nameof(SyncActivityTable)}");
            }
        }

        private void RefreshFilteredActivities()
        {
            if (ActivityList == null)
                return;
            if (ShowIncomingActivity == null || ShowIncomingActivity == true)
            {
                ActivityList.ItemsSource = ViewModel.SelectedSync?.SyncActivities;
            }
            else if (ViewModel.SelectedSync is not null)
            {
                SubscribeToActivities(ViewModel.SelectedSync);
                ActivityList.ItemsSource = _outGoingActivities;
            }
            else
            {
                ActivityList.ItemsSource = null;
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
                else
                {
                    Logger.Log(Logger.Level.Warning, $"Directory does not exist: {activity.ParentFolderPath}");
                }
            }
            else
            {
                Logger.Log(Logger.Level.Warning, $"Unexpected call, sender is not a HyperlinkButton or DataContext is not a SyncActivity");
            }
        }

        private void UpToDateLink_Click(object sender, RoutedEventArgs e)
        {
            ((App)Application.Current).CurrentWindow?.AppWindow.Hide();
        }
    }
    public class ItemTypeDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FileTemplate { get; set; }
        public DataTemplate? DirectoryTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item == null)
                return null;

            Types.NodeType nodeType;
            if (item is SyncActivity syncActivity)
            {
                nodeType = syncActivity.NodeType;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unexpected type in SelectTemplateCore");
                return null;
            }

            switch (nodeType)
            {
                case Types.NodeType.Directory:
                    return DirectoryTemplate;
                default:
                    return FileTemplate;
            }
        }
    }
}

