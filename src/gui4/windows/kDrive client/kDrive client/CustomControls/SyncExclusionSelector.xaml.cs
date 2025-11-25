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
using System.ComponentModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /// <summary>
    /// SyncExclusionSelector
    /// UI control allowing the user to define which remote folders are EXCLUDED from a sync.
    ///
    /// Representation:
    ///  - The remote folder hierarchy (directories only) is displayed as a tree.
    ///  - Each directory has a tri-state checkbox: Checked (included), Unchecked (excluded), Indeterminate (some descendants excluded).
    ///  - Each directory contains a synthetic "file" child item representing all files directly in that folder. Its checkbox:
    ///        * Enabled only when ALL subdirectories are excluded.
    ///        * Mirrors selection logic for directly contained files.
    ///  - The exclusion list stored on the server is the list of UNSELECTED (excluded) nodes.
    ///
    /// Important invariants:
    ///  - If a directory is explicitly unchecked -> all its descendants are implicitly excluded (we do not need to load them).
    ///  - If a directory is explicitly checked -> all its descendants are implicitly included unless individually excluded.
    ///  - Indeterminate means at least one descendant differs (some excluded, some included).
    ///
    /// Persistence flow:
    ///  - On load we fetch current excluded (blacklisted) node ids + their paths.
    ///  - UI tree is rebuilt and tri-state states are computed from that exclusion map.
    ///  - User modifies selections (no server calls yet) and HasPendingChanges becomes true if current UI differs from original exclusion set.
    ///  - SaveChanges() uploads the new list of excluded node ids computed from current tri-state states.
    ///  - CancelChanges() reloads original data from server.
    ///
    /// </summary>
    public sealed partial class SyncExclusionSelector : UserControl
    {
        #region Private fields

        // Node IDs currently excluded on the server for this sync
        private List<NodeId> _excludedNodeIds = new List<NodeId>();

        // Map of excluded node id -> full remote path (needed to infer exclusion state for unloaded descendants)
        private Dictionary<NodeId, string> _excludedNodePathsMap = new Dictionary<NodeId, string>();

        // Root level items displayed in the TreeView (children of the logical root folder)
        private ObservableCollection<TreeItem> _rootLevelItems { get; } = new ObservableCollection<TreeItem>();

        // Flag guarding recursive event cascades while we programmatically change selection states
        private bool _isBulkSelectionPropagation = false;

        #endregion

        #region Constructor / lifecycle
        public SyncExclusionSelector()
        {
            InitializeComponent();
            Loaded += OnControlLoadedAsync;
        }

        // Initial load of data once control is displayed
        private async void OnControlLoadedAsync(object sender, RoutedEventArgs e)
        {
            await ReloadAsync();
        }
        #endregion

        #region Dependency Properties
        public DbId UserDbId
        {
            get => (DbId)GetValue(UserDbIdProperty);
            set => SetValue(UserDbIdProperty, value);
        }
        public static readonly DependencyProperty UserDbIdProperty =
            DependencyProperty.Register(nameof(UserDbId), typeof(DbId), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        public DriveId DriveId
        {
            get => (DriveId)GetValue(DriveIdProperty);
            set => SetValue(DriveIdProperty, value);
        }
        public static readonly DependencyProperty DriveIdProperty =
            DependencyProperty.Register(nameof(DriveId), typeof(DriveId), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        public NodeId RemoteNodeId
        {
            get => (NodeId)GetValue(RemoteNodeIdProperty);
            set => SetValue(RemoteNodeIdProperty, value);
        }
        public static readonly DependencyProperty RemoteNodeIdProperty =
            DependencyProperty.Register(nameof(RemoteNodeId), typeof(NodeId), typeof(SyncExclusionSelector), new PropertyMetadata(""));

        public DbId? SyncDbId
        {
            get => (DbId?)GetValue(SyncDbIdProperty);
            set => SetValue(SyncDbIdProperty, value);
        }
        public static readonly DependencyProperty SyncDbIdProperty =
            DependencyProperty.Register(nameof(SyncDbId), typeof(DbId), typeof(SyncExclusionSelector), new PropertyMetadata(null, OnSyncDbIdChanged));

        public bool HasPendingChanges
        {
            get => (bool)GetValue(HasPendingChangesProperty);
            set => SetValue(HasPendingChangesProperty, value);
        }
        public static readonly DependencyProperty HasPendingChangesProperty =
            DependencyProperty.Register(nameof(HasPendingChanges), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(false));

        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }
        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(true));

        // Internal root item
        private TreeItem RootTreeItem
        {
            get => (TreeItem)GetValue(RootTreeItemProperty);
            set => SetValue(RootTreeItemProperty, value);
        }
        private static readonly DependencyProperty RootTreeItemProperty =
            DependencyProperty.Register(nameof(RootTreeItem), typeof(TreeItem), typeof(SyncExclusionSelector), new PropertyMetadata(null));
        #endregion

        #region Compatibility binding properties (preserve original XAML bindings)
        // Expose original property names expected by existing XAML for backward compatibility.
        // They map to the renamed internal members.
        public ObservableCollection<TreeItem> RootItems => _rootLevelItems;
        public TreeItem? RootItem => RootTreeItem;
        public bool StatePropagationInProgress => _isBulkSelectionPropagation;
        #endregion

        #region Public methods
        public async Task SaveChanges()
        {
            if (IsLoading) return;
            if (SyncDbId is null)
            {
                Logger.Log(Logger.Level.Error, "Cannot save sync exclusion changes: SyncDbId is null.");
                return;
            }
            IsLoading = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            await commService.SetBlacklistedNodeIdList(SyncDbId.Value, GetExcludedNodeIds(), CancellationToken.None);
            await ReloadAsync();
            IsLoading = false;
        }

        public async Task CancelChanges()
        {
            HasPendingChanges = false;
            await ReloadAsync();
        }

        // Compute the list of node IDs that are currently excluded (unchecked) in the UI tree
        public List<NodeId> GetExcludedNodeIds()
        {
            List<NodeId> excluded = new List<NodeId>();
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
            if (parent.Type == NodeType.File) return [];

            if (parent.IsSelected is not null)
            {
                if (parent.IsSelected == false) // Explicitly excluded -> all descendants implicitly excluded
                    return [parent.Node.NodeId];
                else // Explicitly included -> descendants included
                    return [];
            }
            else if (parent._childrenLoaded) // Indeterminate and children loaded: evaluate children individually
            {
                List<NodeId> excluded = new List<NodeId>();
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
                List<NodeId> excluded = _excludedNodePathsMap.Where(pair => pair.Value.StartsWith(parent.Node.Path)).Select(pair => pair.Key).ToList();
                if (excluded.Count == 0)
                    Logger.Log(Logger.Level.Error, $"Logic error: parent node {parent.Node.NodeId} - {parent.Node.Name} is indeterminate but all children are selected.");
                return excluded;
            }
        }
        #endregion

        #region DependencyProperty change handlers
        private static async void OnSyncDbIdChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
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
            await RefreshExcludedNodesAsync();
            await BuildRootLevelItemsAsync();
            IsLoading = false;
        }

        // (Re)build root level items under the logical root folder using current exclusion map
        public async Task BuildRootLevelItemsAsync()
        {
            _isBulkSelectionPropagation = true;

            if (RootTreeItem is not null)
                RootTreeItem.Children.Clear();
            _rootLevelItems.Clear();

            // Logical root node
            Node rootNode = new Node(RemoteNodeId, "", -1, "", "", UserDbId, DriveId);
            RootTreeItem = new TreeItem(rootNode, UserDbId, DriveId, null, _excludedNodePathsMap);

            // Remove synthetic file item at root (not needed at top level)
            var fileItem = RootTreeItem.Children.FirstOrDefault(i => i.Type == NodeType.File);
            if (fileItem is not null) RootTreeItem.Children.Remove(fileItem);

            await RootTreeItem.LoadImmediateChildrenAsync();

            _rootLevelItems.AddRange(RootTreeItem.Children);

            _isBulkSelectionPropagation = false;
            HasPendingChanges = false;
        }

        // Refresh exclusion map from server
        private async Task RefreshExcludedNodesAsync()
        {
            DbId syncDbId = SyncDbId ?? -1;
            _excludedNodeIds.Clear();

            if (syncDbId != -1) // Existing sync -> load exclusions
            {
                var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
                _excludedNodeIds = await commService.GetBlacklistedNodeIdList(syncDbId, CancellationToken.None) ?? new();
                await RefreshExcludedNodePathsAsync();
            }
            else // New sync -> no exclusions
            {
                _excludedNodePathsMap.Clear();
            }
        }

        // Ensure path map for excluded nodes is up to date (add new)
        private async Task RefreshExcludedNodePathsAsync()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            async Task loadPath(NodeId nodeId)
            {
                var nodeInfo = await commService.GetNodeInfo(UserDbId, DriveId, nodeId, CancellationToken.None);
                if (nodeId is null || nodeInfo is null || nodeInfo.Path == "")
                {
                    Logger.Log(Logger.Level.Warning, "Failed to load node info for nodeId: " + nodeId);
                    return;
                }
                _excludedNodePathsMap.TryAdd(nodeId, nodeInfo.Path);
            }
            var newExcludedNodePathsMap = _excludedNodePathsMap.Where(pair => _excludedNodeIds.Contains(pair.Key)).ToDictionary(); // Remove all the nodes that are not blacklisted anymore.
            _excludedNodePathsMap.Clear(); // Cannot just use "=" because of references held by TreeItems.
            foreach (var pair in newExcludedNodePathsMap)
            {
                _excludedNodePathsMap.TryAdd(pair.Key, pair.Value);
            }

            // Fetch path for newly excluded nodes
            List<Task> loadTasks = new List<Task>();
            foreach (var nodeId in _excludedNodeIds.Where(id => !_excludedNodePathsMap.ContainsKey(id)))
            {
                loadTasks.Add(loadPath(nodeId));
            }
            await Task.WhenAll(loadTasks);
        }
        #endregion

        #region TreeView events
        private async void TreeView_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            _isBulkSelectionPropagation = true;
            if (args.Item is TreeItem item)
                await item.LoadImmediateChildrenAsync();
            _isBulkSelectionPropagation = false;
        }
        #endregion

        #region Checkbox event handlers
        private void CheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null && !_isBulkSelectionPropagation)
            {
                _isBulkSelectionPropagation = true;
                treeItem.IsSelected = true;
                SelectAllDescendants(treeItem);
                _isBulkSelectionPropagation = false;
                if (!IsLoading) UpdateHasPendingChanges();
            }
        }

        private void CheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null && !_isBulkSelectionPropagation)
            {
                _isBulkSelectionPropagation = true;
                treeItem.IsSelected = false;
                DeselectAllDescendants(treeItem);
                _isBulkSelectionPropagation = false;
                if (!IsLoading) UpdateHasPendingChanges();
            }
        }

        private void CheckBox_Indeterminate(object sender, RoutedEventArgs e)
        {
            var treeItem = ExtractTreeItemFromSender(sender);
            if (treeItem is not null && !_isBulkSelectionPropagation && treeItem.IsSelected is not null)
            {
                if (treeItem.IsSelected.Value)
                    CheckBox_Unchecked(sender, e);
                else
                    CheckBox_Checked(sender, e);
            }
        }
        #endregion

        #region Selection propagation helpers
        private static TreeItem? ExtractTreeItemFromSender(object sender)
        {
            var control = sender as Control;
            return control?.DataContext as TreeItem ?? control?.Tag as TreeItem;
        }

        private void SelectAllDescendants(TreeItem parent)
        {
            foreach (var child in parent.Children)
            {
                child.IsSelected = true;
                SelectAllDescendants(child);
            }
        }

        private void DeselectAllDescendants(TreeItem parent)
        {
            foreach (var child in parent.Children.Where(c => c.Type != NodeType.File))
            {
                child.IsSelected = false;
                DeselectAllDescendants(child);
            }
            // Deselect synthetic file child if present last (to avoid re-enabling it during recursion)
            var fileChild = parent.Children.FirstOrDefault(c => c.Type == NodeType.File);
            if (fileChild is not null)
                fileChild.IsSelected = false;
        }
        #endregion

        #region Misc UI helpers

        // Lazy load size
        private async void SizeContentLoader_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if ((sender as Control)?.DataContext is TreeItem treeItem)
                if (treeItem.Node.Size == -1)
                    await treeItem.Node.LoadSize();
        }

        // Recompute HasPendingChanges by comparing current excluded ids with original server exclusion set
        private void UpdateHasPendingChanges()
        {
            if (HasPendingChanges) return;
            if (RootTreeItem is null)
            {
                HasPendingChanges = false;
                return;
            }
            var currentExcluded = GetExcludedDescendantNodeIds(RootTreeItem);
            var currentSet = new HashSet<string>(currentExcluded);
            var excludedSet = new HashSet<string>(_excludedNodeIds);
            HasPendingChanges = !currentSet.SetEquals(excludedSet);
        }
        #endregion
    }

    /// <summary>
    /// TreeItem wraps a Node and adds UI-only state: tri-state selection plus lazy child loading logic.
    /// A synthetic File item represents all direct files within a directory.
    /// </summary>
    public class TreeItem : UISafeObservableObject
    {
        #region Private fields
        private DbId _userDbId = 0;
        private DriveId _driveId;
        private bool _isLoadingChildren = false;
        public bool _childrenLoaded = false;
        private bool _isEnabled = true;
        private bool? _isSelected = true; // true=include, false=exclude, null=partial
        private Dictionary<NodeId, string> _excludedNodes;
        #endregion

        #region Public properties
        public Node Node { get; private set; }

        public bool IsLoadingChildren
        {
            get => _isLoadingChildren;
            private set => SetPropertyInUIThread(ref _isLoadingChildren, value);
        }

        public bool IsEnabled
        {
            get => _isEnabled;
            private set => SetPropertyInUIThread(ref _isEnabled, value);
        }

        public bool? IsSelected
        {
            get => _isSelected;
            set
            {
                SetPropertyInUIThread(ref _isSelected, value);
                UpdateSyntheticFileItemSelectionState();
            }
        }

        public ObservableCollection<TreeItem> Children { get; } = new ObservableCollection<TreeItem>();
        public TreeItem? ParentItem { get; }
        public NodeType Type { get; }
        #endregion

        public TreeItem(Node node, DbId userDbId, DriveId driveId, TreeItem? parentItem, Dictionary<NodeId, string> excluded, NodeType type = NodeType.Directory)
        {
            Node = node;
            ParentItem = parentItem;
            _userDbId = userDbId;
            _driveId = driveId;
            Type = type;
            _excludedNodes = excluded;

            if (Type == NodeType.Directory)
            {
                // Synthetic file child representing files directly inside this folder
                Children.Add(new TreeItem(new Node("-1", "", 0, "", "", _userDbId, _driveId), 0, 0, this, _excludedNodes, NodeType.File));
                Children.CollectionChanged += OnChildrenCollectionChanged;
                RegisterSelectionCallbacks(Children);
                UpdateSyntheticFileItemSelectionState();
            }
            if (Type == NodeType.File)
            {
                Node.Name = "Fichiers du dossier " + (ParentItem?.Node.Name ?? "");
                Node.Path = parentItem?.Node.Path + System.IO.Path.DirectorySeparatorChar + Node.Name;
                _childrenLoaded = true;
            }
            RecomputeSelectionFromExclusionMap();
        }

        // Recompute tri-state based on exclusion map
        public void RecomputeSelectionFromExclusionMap()
        {
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
            RegisterSelectionCallbacks(e.NewItems?.Cast<TreeItem>().ToList());
        }

        private void RegisterSelectionCallbacks(IEnumerable<TreeItem>? newItems)
        {
            if (newItems != null)
                foreach (var item in newItems)
                    if (item is TreeItem treeItem)
                        treeItem.WhenAnyPropertyChanged(nameof(TreeItem.IsSelected)).Subscribe(changedItem =>
                        {
                            if (changedItem?.Type != NodeType.File) UpdateSyntheticFileItemSelectionState();
                            UpdateDirectoryTriStateFromChildren();
                        });
            UpdateSyntheticFileItemSelectionState();
        }

        // Maintain synthetic file child state based on directory selection + descendant selections
        private void UpdateSyntheticFileItemSelectionState()
        {
            if (Type == NodeType.File) return;

            var fileItem = Children.FirstOrDefault(c => c.Type == NodeType.File);
            if (fileItem == null) return;

            if (!(IsSelected ?? true))
            {
                fileItem.IsSelected = false;
                fileItem.IsEnabled = true;
                return;
            }

            bool hasSelectedDirectoryChild = Children.Where(c => c.Type == NodeType.Directory).Any(c => c.IsSelected ?? true);
            if (hasSelectedDirectoryChild) fileItem.IsSelected = hasSelectedDirectoryChild;
            fileItem.IsEnabled = !hasSelectedDirectoryChild; // enable only if no selected subfolder
        }

        // Refresh directory tri-state based on children states
        private void UpdateDirectoryTriStateFromChildren()
        {
            if (Type == NodeType.File) return;

            if (Children.All(c => c.IsSelected == true))
            {
                IsSelected = true;
                return;
            }
            else if (Children.All(c => c.IsSelected == false))
            {
                IsSelected = false;
                return;
            }
            else
            {
                IsSelected = null; // partial
            }
        }

        // Lazy load direct child directories
        public async Task LoadImmediateChildrenAsync()
        {
            if (_childrenLoaded || Type == NodeType.File) return;
            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(_userDbId, _driveId, Node.NodeId, CancellationToken.None);
            if (nodes != null)
            {
                foreach (Node node in nodes)
                    Children.Add(new TreeItem(node, _userDbId, _driveId, this, _excludedNodes));
                _childrenLoaded = true;
            }
            else
                Logger.Log(Logger.Level.Error, "Failed to load nodes for SyncExclusionSelector.");
            IsLoadingChildren = false;
        }
    }

    /// <summary>
    /// Chooses between directory and synthetic file templates.
    /// </summary>
    public class NodeTypeTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? DirectoryTemplate { get; set; }
        public DataTemplate? FileTemplate { get; set; }
        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is TreeItem treeItem)
                return treeItem.Type == NodeType.Directory ? DirectoryTemplate : FileTemplate;
            return base.SelectTemplateCore(item);
        }
    }
}
