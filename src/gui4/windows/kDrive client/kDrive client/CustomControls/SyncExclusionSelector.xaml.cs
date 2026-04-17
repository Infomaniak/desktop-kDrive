using DynamicData;
using DynamicData.Binding;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Linq;
using System.Reactive.Disposables;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /*
    //  SyncExclusionSelector: A UI control for managing which remote folders are EXCLUDED from synchronization.
    //  
    //  Purpose:
    //  Allows users to define a blacklist of remote folders (and their contents) that should NOT be synced locally.
    //  The UI presents a tree view of the remote folder hierarchy, with tri-state checkboxes for inclusion/exclusion.
    //  
    //  Tree Structure & Behavior:
    //  - Displays only directories (no files) in a hierarchical tree.
    //  - Each folder has a tri-state checkbox:
    //      • Checked: Included in sync -> all descendants are implicitly included.
    //      • Unchecked: Excluded from sync -> all descendants are implicitly excluded.
    //      • Indeterminate (-): Mixed state — some descendants are excluded, some are included.
    //  
    //  Persistence & State Management:
    //  - On load: Fetches the current list of excluded node IDs + their paths from the server.
    //  - UI tree is built and tri-state states are computed from this exclusion map.
    //  - User changes are tracked locally; no server calls until SaveChanges() is called.
    //  - HasPendingChanges becomes true when UI differs from the original server state.
    //  - SaveChanges(): Uploads the new list of excluded node IDs (unselected nodes).
    //  - CancelChanges(): Reverts UI to original server state.
    //  
    //  Notes:
    //  - The root folder cannot be excluded.
    //  - Children are lazily loaded on expand (TreeView_Expanding).
    //  - File size is lazily loaded on view (SizeContentLoader_DataContextChanged).
    //  - HasPendingChanges is updated automatically when selections change.
    //  - Uses DynamicData for reactive collections and binding.
    //  - AccessDenied nodes are read-only and cannot be modified.
    */
    public sealed partial class SyncExclusionSelector : UserControl
    {
        #region Private fields

        // Node IDs currently excluded on the server for this sync
        private List<NodeId> _excludedNodeIds = [];

        // Map of excluded node id -> full remote path (needed to infer exclusion state for unloaded descendants)
        private readonly Dictionary<NodeId, string> _excludedNodePathsMap = [];

        // Root level items displayed in the TreeView (children of the logical root folder)
        private readonly ObservableCollection<TreeItem> _rootLevelItems = [];

        #endregion

        #region Constructor / lifecycle
        public SyncExclusionSelector()
        {
            InitializeComponent();
        }

        // Initial load of data once control is displayed
        private async void SyncExclusionSelector_Loaded(object sender, RoutedEventArgs e)
        {
            await ReloadAsync();
        }

        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            ClearAllTreeItems();
        }
        #endregion

        #region Dependency Properties
        private DbId UserDbId
        {
            get => (DbId)GetValue(UserDbIdProperty);
            set => SetValue(UserDbIdProperty, value);
        }
        public static readonly DependencyProperty UserDbIdProperty =
            DependencyProperty.Register(nameof(UserDbId), typeof(DbId), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        private DriveId DriveId
        {
            get => (DriveId)GetValue(DriveIdProperty);
            set => SetValue(DriveIdProperty, value);
        }
        public static readonly DependencyProperty DriveIdProperty =
            DependencyProperty.Register(nameof(DriveId), typeof(DriveId), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        private NodeId RemoteRootNodeId
        {
            get => (NodeId)GetValue(RemoteNodeIdProperty);
            set => SetValue(RemoteNodeIdProperty, value);
        }
        public static readonly DependencyProperty RemoteNodeIdProperty =
            DependencyProperty.Register(nameof(RemoteRootNodeId), typeof(NodeId), typeof(SyncExclusionSelector), new PropertyMetadata(""));

        public ISync? Sync
        {
            get => (ISync?)GetValue(SyncProperty);
            set
            {
                SetValue(SyncProperty, value);
                if (value is not null)
                {
                    DriveId = value.Drive.DriveId;
                    UserDbId = value.Drive.UserDbId;
                    RemoteRootNodeId = value.RemoteNodeId;
                }
            }
        }
        public static readonly DependencyProperty SyncProperty =
            DependencyProperty.Register(nameof(Sync), typeof(ISync), typeof(SyncExclusionSelector), new PropertyMetadata(null, OnSyncChanged));

        public bool HasPendingChanges
        {
            get => (bool)GetValue(HasPendingChangesProperty);
            set
            {
                SetValue(HasPendingChangesProperty, value);
                if (CanShowSaveMenu)
                    ShowSaveMenu = value;
            }
        }
        public static readonly DependencyProperty HasPendingChangesProperty =
            DependencyProperty.Register(nameof(HasPendingChanges), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(false));

        public bool HasSubDirectory
        {
            get => (bool)GetValue(HasSubDirectoryProperty);
            set => SetValue(HasSubDirectoryProperty, value);

        }
        public static readonly DependencyProperty HasSubDirectoryProperty =
            DependencyProperty.Register(nameof(HasSubDirectory), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(false));

        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }
        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(true));

        public bool CanShowSaveMenu
        {
            get => (bool)GetValue(CanShowSaveMenuProperty);
            set
            {
                SetValue(CanShowSaveMenuProperty, value);
                if (!value)
                    ShowSaveMenu = false;
            }
        }

        public static readonly DependencyProperty CanShowSaveMenuProperty =
            DependencyProperty.Register(nameof(CanShowSaveMenu), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(false));

        // Internal root item
        private TreeItem? RootTreeItem
        {
            get => (TreeItem?)GetValue(RootTreeItemProperty);
            set => SetValue(RootTreeItemProperty, value);
        }
        private static readonly DependencyProperty RootTreeItemProperty =
            DependencyProperty.Register(nameof(RootTreeItem), typeof(TreeItem), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        public bool ShowSaveMenu
        {
            get => (bool)GetValue(ShowSaveMenuProperty);
            set => SetValue(ShowSaveMenuProperty, value);
        }
        public static readonly DependencyProperty ShowSaveMenuProperty =
            DependencyProperty.Register(nameof(ShowSaveMenu), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(false));

        #endregion

        #region Public methods
        public async Task SaveChanges()
        {
            if (IsLoading)
                return;

            if (Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot save sync exclusion changes: Sync is null.");
                return;
            }

            if (Sync is ViewModels.Sync dbSync)
            {
                IsLoading = true;
                if (!await dbSync.SetExcludedNodeIds(GetExcludedNodeIds()))
                {
                    Logger.Log(Logger.Level.Warning, "Failed to save BlacklistedNodeIdList");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }
                await ReloadAsync();
                IsLoading = false;
            }
            else if (Sync is NewSync tmpSync)
            {
                tmpSync.ExcludedNodeIds.Clear();
                tmpSync.ExcludedNodeIds.AddRange(GetExcludedNodeIds());
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Cannot save sync exclusion changes: Unsupported Sync type.");
            }
        }

        public async Task CancelChanges()
        {
            HasPendingChanges = false;
            await ReloadAsync();
        }

        // Compute the list of node IDs that are currently excluded (unchecked) in the UI tree
        public List<NodeId> GetExcludedNodeIds()
        {
            List<NodeId> excluded = [];
            foreach (var item in _rootLevelItems)
            {
                excluded.AddRange(GetExcludedDescendantNodeIds(item));
            }
            return excluded;
        }
        #endregion

        #region Exclusion computation helpers
        // Recursively accumulate excluded node IDs.
        private List<NodeId> GetExcludedDescendantNodeIds(TreeItem parent)
        {
            if (parent.Node.AccessDenied)
                return [];

            if (parent.IsSelected is not null) // If the parent is explicitly included or excluded (ie. not indeterminate)
            {
                if (parent.IsSelected == false) // Explicitly excluded -> all descendants implicitly excluded
                    return [parent.Node.NodeId];
                else // Explicitly included -> descendants included
                    return [];
            }
            else if (parent.ChildrenLoaded) // Indeterminate and children loaded: evaluate children individually
            {
                List<NodeId> excluded = [];
                foreach (var child in parent.Children)
                {
                    excluded.AddRange(GetExcludedDescendantNodeIds(child));
                }
                if (excluded.Count == 0)
                    Logger.Log(Logger.Level.Error, $"Logic error: parent node {parent.Node.NodeId} - {parent.Node.Name} is indeterminate but all children are selected.");
                return excluded;
            }
            else
            {
                // Indeterminate but children not loaded: look up exclusion map for any descendant path starting with this path
                List<NodeId> excluded = [.. _excludedNodePathsMap.Where(pair => pair.Value.StartsWith(parent.Node.Path)).Select(pair => pair.Key)];
                if (excluded.Count == 0)
                    Logger.Log(Logger.Level.Error, $"Logic error: parent node {parent.Node.NodeId} - {parent.Node.Name} is indeterminate but all children are selected.");
                return excluded;
            }
        }
        #endregion

        #region DependencyProperty change handlers
        private static async void OnSyncChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SyncExclusionSelector control && control.IsLoaded)
                await control.ReloadAsync();
        }
        #endregion

        #region Loading / refreshing
        // Reload full state: exclusion map + tree items
        private async Task ReloadAsync()
        {
            IsLoading = true;
            ClearAllTreeItems();
            if (!await RefreshExcludedNodesAsync())
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                IsLoading = false;
                return;
            }
            await BuildRootLevelItemsAsync();
            IsLoading = false;
        }

        // Clear all tree items and dispose them
        private void ClearAllTreeItems()
        {
            if (RootTreeItem is not null)
            {
                RootTreeItem.Dispose();
                RootTreeItem = null;
            }
            foreach (var item in _rootLevelItems)
            {
                item.Dispose();
            }
            _rootLevelItems.Clear();
        }

        // (Re)build root level items under the logical root folder using current exclusion map
        public async Task BuildRootLevelItemsAsync()
        {

            ClearAllTreeItems();

            // Logical root node
            Node rootNode = new Node(RemoteRootNodeId, "", -1, "", "", UserDbId, DriveId, false);
            RootTreeItem = new TreeItem(rootNode, UserDbId, DriveId, null, _excludedNodePathsMap);

            await RootTreeItem.LoadImmediateChildrenAsync();
            HasSubDirectory = RootTreeItem.Children.Any();
            _rootLevelItems.AddRange(RootTreeItem.Children);
            List<Task> tasks = [];
            foreach (var item in _rootLevelItems)
            {
                tasks.Add(item.LoadImmediateChildrenAsync());
            }
            await Task.WhenAll(tasks);
            HasPendingChanges = false;
        }

        // Refresh exclusion map from server
        private async Task<bool> RefreshExcludedNodesAsync()
        {
            _excludedNodeIds.Clear();
            if (Sync is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot refresh excluded nodes: Sync is null.");
                return false;
            }

            var res = await Sync.GetExcludedNodeIds();
            if (res is null)
            {
                Logger.Log(Logger.Level.Warning, "Failed to load blaklisted node ids");
                _excludedNodeIds.Clear();
                return false;
            }
            _excludedNodeIds = res;
            return await RefreshExcludedNodePathsAsync();
        }

        // Ensure path map for excluded nodes is up to date (add new)
        private async Task<bool> RefreshExcludedNodePathsAsync()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            async Task<bool> loadPath(NodeId nodeId)
            {
                var getNodeInfoResult = await commService.GetNodeInfo(UserDbId, DriveId, nodeId, CancellationToken.None);
                if (nodeId is not null && getNodeInfoResult.IsSuccess && getNodeInfoResult.Node is not null)
                {
                    _excludedNodePathsMap.TryAdd(nodeId, getNodeInfoResult.Node.Path);
                    return true;
                }

                if (getNodeInfoResult.Cause == ExitCause.NotFound)
                {
                    Logger.Log(Logger.Level.Info, $"Excluded node with id {nodeId} not found on server. It might have been deleted since it was blacklisted.");
                    return true;
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "Failed to load node info for nodeId: " + nodeId);
                    return false;
                }
            }
            var newExcludedNodePathsMap = _excludedNodePathsMap.Where(pair => _excludedNodeIds.Contains(pair.Key)).ToDictionary(); // Remove all the nodes that are not blacklisted anymore.
            _excludedNodePathsMap.Clear(); // Cannot just use "=" because of references held by TreeItems.
            foreach (var pair in newExcludedNodePathsMap)
            {
                _excludedNodePathsMap.TryAdd(pair.Key, pair.Value);
            }

            // Fetch path for newly excluded nodes
            List<Task<bool>> loadTasks = [];
            foreach (var nodeId in _excludedNodeIds.Where(id => !_excludedNodePathsMap.ContainsKey(id)))
            {
                loadTasks.Add(loadPath(nodeId));
            }
            await Task.WhenAll(loadTasks);

            // Check for any failures
            bool results = loadTasks.All(t => t.Result);
            if (!results)
            {
                Logger.Log(Logger.Level.Warning, "Some excluded node paths failed to load.");
                _excludedNodePathsMap.Clear();
            }

            return results;
        }
        #endregion

        #region TreeView events
        private async void TreeView_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            List<Task> tasks = [];
            if (args.Item is TreeItem item)
            {
                foreach (var child in item.Children)
                {
                    tasks.Add(child.LoadImmediateChildrenAsync());
                }
                await Task.WhenAll(tasks);
            }
        }
        #endregion

        #region Checkbox event handlers

        private void CheckBox_Click(object sender, RoutedEventArgs e)
        {
            Logger.Log(Logger.Level.Debug, "CheckBox_Click event triggered.");
            if (e.OriginalSource is CheckBox checkBox)
            {
                if (checkBox.IsChecked == true)
                {
                    CheckBox_Checked(sender);
                }
                else if (checkBox.IsChecked == false)
                {
                    CheckBox_Unchecked(sender);
                }
                else
                {
                    CheckBox_Indeterminate(sender);
                }
            }
        }

        private void CheckBox_Checked(object sender)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null)
            {
                SelectAllDescendants(treeItem, true);
                if (!IsLoading)
                    UpdateHasPendingChanges();
            }
        }

        private void CheckBox_Unchecked(object sender)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null)
            {
                if (treeItem.ParentItem is null) // The root of the drive cannot be excluded
                {
                    CheckBox_Checked(sender);
                    return;
                }

                DeselectAllDescendants(treeItem, true);
                if (!IsLoading)
                    UpdateHasPendingChanges();
            }
        }

        private void CheckBox_Indeterminate(object sender)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null)
            {
                DeselectAllDescendants(treeItem, false);
                treeItem.IsSelected = false;
                if (!IsLoading)
                    UpdateHasPendingChanges();
            }
        }
        #endregion

        #region Selection propagation helpers
        private static TreeItem? ExtractTreeItemFromSender(object sender)
        {
            var control = sender as Control;
            return control?.DataContext as TreeItem;
        }

        private static void SelectAllDescendants(TreeItem parent, bool selectParent = false)
        {
            if (parent.Node.AccessDenied)
                return;

            foreach (var child in parent.Children)
            {
                SelectAllDescendants(child, true);
            }

            if (selectParent)
                parent.IsSelected = true;
        }

        private static void DeselectAllDescendants(TreeItem parent, bool deselectParent = false)
        {
            if (parent.Node.AccessDenied)
                return;

            foreach (var child in parent.Children)
            {
                DeselectAllDescendants(child, true);
            }

            if (deselectParent)
                parent.IsSelected = false;
        }
        #endregion

        #region Misc UI helpers

        // Lazy load size
        private async void SizeContentLoader_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if ((sender as Control)?.DataContext is TreeItem treeItem && treeItem.Node.Size == -1)
                await treeItem.Node.LoadSize();
        }

        // Recompute HasPendingChanges by comparing current excluded ids with original server exclusion set
        private void UpdateHasPendingChanges()
        {
            if (RootTreeItem is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot update HasPendingChanges: RootTreeItem is null.");
                HasPendingChanges = false;
                return;
            }
            var currentExcluded = GetExcludedDescendantNodeIds(RootTreeItem);
            var currentSet = new HashSet<string>(currentExcluded);
            var excludedSet = new HashSet<string>(_excludedNodeIds);
            HasPendingChanges = !currentSet.SetEquals(excludedSet);
        }

        private async void SaveSyncSelection_click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            await SaveChanges();
            if (control is not null)
                control.IsEnabled = true;

        }

        private async void CancelSyncSelection_click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is not null)
                control.IsEnabled = false;
            await CancelChanges();
            if (control is not null)
                control.IsEnabled = true;
        }

        #endregion      
    }

    /// <summary>
    /// TreeItem wraps a Node and adds UI-only state: tri-state selection plus lazy child loading logic.
    /// </summary>
    public class TreeItem : UISafeObservableObject, IDisposable
    {
        #region Private fields
        private readonly DbId _userDbId = 0;
        private readonly DriveId _driveId;
        private bool _isLoadingChildren = false;
        private bool _childrenLoaded = false;
        private bool? _isSelected = true; // true=include, false=exclude, null=partial
        private readonly Dictionary<NodeId, string> _excludedNodes;
        private readonly CompositeDisposable _disposables = [];
        private bool _disposed = false;
        #endregion

        #region Public properties
        public Node Node { get; private set; }

        public bool IsLoadingChildren
        {
            get => _isLoadingChildren;
            private set => SetPropertyInUIThread(ref _isLoadingChildren, value);
        }

        public bool ChildrenLoaded
        {
            get => _childrenLoaded;
            private set => SetPropertyInUIThread(ref _childrenLoaded, value);
        }
        public bool? IsSelected
        {
            get => _isSelected;
            set
            {
                SetPropertyInUIThread(ref _isSelected, value);
            }
        }

        public ObservableCollection<TreeItem> Children { get; } = [];
        public TreeItem? ParentItem { get; }
        #endregion

        public TreeItem(Node node, DbId userDbId, DriveId driveId, TreeItem? parentItem, Dictionary<NodeId, string> excluded)
        {
            Node = node;
            ParentItem = parentItem;
            _userDbId = userDbId;
            _driveId = driveId;
            _excludedNodes = excluded;

            Children.CollectionChanged += OnChildrenCollectionChanged;
            _disposables.Add(Disposable.Create(() => Children.CollectionChanged -= OnChildrenCollectionChanged));

            RegisterSelectionCallbacks(Children);

            RecomputeSelectionFromExclusionMap();
        }

        // Recompute tri-state based on exclusion map
        public void RecomputeSelectionFromExclusionMap()
        {
            if (Node.AccessDenied)
                return;

            if (ParentItem is not null && ParentItem.IsSelected is not null)
            {
                IsSelected = ParentItem.IsSelected;
            }
            else
            {
                if (_excludedNodes.Any(pair => pair.Key == Node.NodeId || pair.Value == Node.Path))
                {
                    IsSelected = false; // this node explicitly excluded
                }
                else if (_excludedNodes.Any(pair => Utility.IsSubPathOf(pair.Value, Node.Path)))
                {
                    IsSelected = null; // some descendant excluded -> partial
                }
                else
                {
                    IsSelected = true; // fully included
                }
            }
            foreach (var child in Children)
            {
                child.RecomputeSelectionFromExclusionMap();
            }
        }

        private void OnChildrenCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            if (e.OldItems != null)
            {
                foreach (TreeItem item in e.OldItems.OfType<TreeItem>())
                {
                    item.Dispose();
                }
            }

            RegisterSelectionCallbacks(e.NewItems?.Cast<TreeItem>().ToList());
        }

        private void RegisterSelectionCallbacks(IEnumerable<TreeItem>? newItems)
        {
            if (newItems != null)
                foreach (var item in newItems)
                    if (item is TreeItem treeItem)
                    {
                        var subscription = treeItem.WhenAnyPropertyChanged(nameof(TreeItem.IsSelected)).Subscribe(changedItem =>
                        {
                            UpdateDirectoryTriStateFromChildren();
                        });
                        _disposables.Add(subscription);
                    }
        }


        // Refresh directory tri-state based on children states
        private void UpdateDirectoryTriStateFromChildren()
        {
            if (Children.All(c => c.IsSelected == true || c.Node.AccessDenied))
            {
                IsSelected = true;
            }
            else if (Children.All(c => c.IsSelected == false || c.Node.AccessDenied))
            {
                IsSelected = null;
            }
            else
            {
                IsSelected = null; // partial
            }
        }

        // Lazy load direct child directories
        public async Task LoadImmediateChildrenAsync()
        {
            if (_childrenLoaded || Node.AccessDenied)
                return;
            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(_userDbId, _driveId, Node.NodeId, CancellationToken.None);
            if (nodes is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to load nodes for SyncExclusionSelector.");
                IsLoadingChildren = false;
                Utility.ShowUnexpectedErrorTeachingTip();
                _childrenLoaded = true;
                IsLoadingChildren = false;
                return;
            }

            foreach (Node node in nodes)
                Children.Add(new TreeItem(node, _userDbId, _driveId, this, _excludedNodes));
            _childrenLoaded = true;
            IsLoadingChildren = false;
        }

        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;

            foreach (var child in Children)
            {
                child.Dispose();
            }
            Children.CollectionChanged -= OnChildrenCollectionChanged;

            Children.Clear();

            _disposables?.Dispose();
        }
    }

    public partial class FolderTreeViewItemTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? DirectoryTemplate { get; set; }
        public DataTemplate? AccessDeniedTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is not TreeItem treeItem)
                return base.SelectTemplateCore(item);

            return treeItem.Node.AccessDenied ? AccessDeniedTemplate : DirectoryTemplate;
        }
    }
}
