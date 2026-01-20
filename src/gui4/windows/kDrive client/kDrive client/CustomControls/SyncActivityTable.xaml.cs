using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Reactive.Linq;
using System.Threading.Tasks;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncActivityTable : UserControl
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel => _viewModel;

        private readonly ObservableCollection<SyncFileItem> _outGoingActivities = [];
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
                .Filter(a => a.Direction == SyncDirection.Up)
                .Sort(SortExpressionComparer<SyncFileItem>.Ascending(a => a.Timestamp))
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
            if (sender is HyperlinkButton btn && btn.DataContext is SyncFileItem activity)
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
        private void ItemErrorIcon_PointerPressed(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            Frame? frame = Utility.GetFrame(this);
            if (frame is not null)
            {
                Logger.Log(Logger.Level.Info, "Navigating to ErrorPage.");
                frame.Navigate(typeof(Pages.ErrorPage));
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Could not find Frame in visual tree to navigate to error page");
            }

        }

        private void OpenInExplorer_Click(object sender, RoutedEventArgs e)
        {

        }

        private void OpenOnline_Click(object sender, RoutedEventArgs e)
        {

        }

        private void CopyPublicLink_Click(object sender, RoutedEventArgs e)
        {

        }
    }
    public partial class ItemTypeDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FileTemplate { get; set; }
        public DataTemplate? DirectoryTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item == null)
                return null;

            Types.NodeType nodeType;
            if (item is SyncFileItem syncActivity)
            {
                nodeType = syncActivity.Type;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unexpected type in SelectTemplateCore");
                return null;
            }

            return nodeType switch
            {
                Types.NodeType.Directory => DirectoryTemplate,
                _ => FileTemplate,
            };
        }
    }

    public partial class ActivityStatusDataTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SynchronizedTemplate { get; set; }
        public DataTemplate? SynchronizingTemplate { get; set; }
        public DataTemplate? SynchronizationErrorTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item == null)
                return null;

            Types.SyncFileStatus syncFileStatus;
            if (item is SyncFileItem syncActivity)
            {
                syncFileStatus = syncActivity.Status;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unexpected type in SelectTemplateCore");
                return null;
            }

            return syncFileStatus switch
            {
                SyncFileStatus.Unknown => SynchronizationErrorTemplate,
                SyncFileStatus.Error => SynchronizationErrorTemplate,
                SyncFileStatus.Success => SynchronizedTemplate,
                SyncFileStatus.Conflict => SynchronizationErrorTemplate,
                SyncFileStatus.Inconsistency => SynchronizationErrorTemplate,
                SyncFileStatus.Ignored => SynchronizationErrorTemplate,
                SyncFileStatus.Syncing => SynchronizingTemplate,
                _ => SynchronizationErrorTemplate,
            };
        }
    }
}

