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

    public sealed partial class SyncExclusionSelector : UserControl
    {
        public ObservableCollection<TreeItem> RootItems { get; } = new ObservableCollection<TreeItem>();

        // The lists of special sync nodes (loaded from the server)
        private List<NodeId> BlacklistedNodeIds = new List<NodeId>();

        // The paths of currently excluded nodes (Blacklist and Undecided)
        private Dictionary<NodeId, string> BlacklistedNodesPaths = new Dictionary<NodeId, string>();

        // Flag to prevent recursive state propagation during selection changes
        private bool StatePropagationInProgress { get; set; } = false;
        public SyncExclusionSelector()
        {
            this.InitializeComponent();
            Loaded += SyncExclusionSelector_Loaded;

        }
        private async void SyncExclusionSelector_Loaded(object sender, RoutedEventArgs e)
        {
            await LoadTreeViewData();
        }

        // DependencyProperties
        public DbId UserDbId
        {
            get => (DbId)GetValue(UserDbIdProperty);
            set => SetValue(UserDbIdProperty, value);
        }

        public static readonly DependencyProperty UserDbIdProperty =
            DependencyProperty.Register(
                nameof(UserDbId),
                typeof(DbId),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(null)
            );

        public DriveId DriveId
        {
            get => (DriveId)GetValue(DriveIdProperty);
            set => SetValue(DriveIdProperty, value);
        }

        public static readonly DependencyProperty DriveIdProperty =
            DependencyProperty.Register(
                nameof(DriveId),
                typeof(DriveId),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(null)
            );

        public NodeId RemoteNodeId
        {
            get => (NodeId)GetValue(RemoteNodeIdProperty);
            set => SetValue(RemoteNodeIdProperty, value);
        }

        public static readonly DependencyProperty RemoteNodeIdProperty =
            DependencyProperty.Register(
                nameof(RemoteNodeId),
                typeof(NodeId),
                typeof(SyncExclusionSelector),
                new PropertyMetadata("")
            );

        public DbId? SyncDbId
        {
            get => (DbId?)GetValue(SyncDbIdProperty);
            set => SetValue(SyncDbIdProperty, value);
        }

        public static readonly DependencyProperty SyncDbIdProperty =
            DependencyProperty.Register(
                nameof(SyncDbId),
                typeof(DbId),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(null, OnSyncDbIdChanged)
            );

        public bool HasPendingChanges
        {
            get => (bool)GetValue(HasPendingChangesProperty);
            set => SetValue(HasPendingChangesProperty, value);
        }

        public static readonly DependencyProperty HasPendingChangesProperty =
            DependencyProperty.Register(
                nameof(HasPendingChanges),
                typeof(bool),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(false)
            );

        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }

        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(
                nameof(IsLoading),
                typeof(bool),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(true)
            );

        public TreeItem RootItem
        {
            get => (TreeItem)GetValue(RootItemProperty);
            set => SetValue(RootItemProperty, value);
        }

        public static readonly DependencyProperty RootItemProperty =
            DependencyProperty.Register(
                nameof(RootItem),
                typeof(TreeItem),
                typeof(SyncExclusionSelector),
                new PropertyMetadata(null)
            );

        // Public methods
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
            await commService.SetBlacklistedNodeIdList(SyncDbId ?? -1, UnselectedNodesIds(), CancellationToken.None);
            await LoadTreeViewData();
            IsLoading = false;
        }

        public async Task CancelChanges()
        {
            HasPendingChanges = false;
            await LoadTreeViewData();
        }

        public List<NodeId> UnselectedNodesIds()
        {
            List<NodeId> unselectedNodes = new List<NodeId>();
            foreach (var item in RootItems)
            {
                unselectedNodes.AddRange(UnselectedChildNodes(item));
            }
            return unselectedNodes;
        }

        private List<NodeId> UnselectedChildNodes(TreeItem parent)
        {
            if (parent.Type == NodeType.File) return [];

            if (parent.IsSelected is not null)
            {
                if (parent.IsSelected == false) // If the item is unselected all the children are too
                    return [parent.Node.NodeId];
                else // If the item is selected all the children are too
                    return [];
            }
            else if (parent.ChildrenLoaded) // If the item is indeterminate and the children are loaded we check them one by one
            {
                List<NodeId> unselectedNodes = new List<NodeId>();
                foreach (var child in parent.Children)
                {
                    unselectedNodes.AddRange(UnselectedChildNodes(child));
                }
                if (unselectedNodes.Count == 0)
                    Logger.Log(Logger.Level.Error, $"Logic error in UnselectedChildNodes: parent node {parent.Node.NodeId} - {parent.Node.Name} is indeterminate but all children are selected.");

                return unselectedNodes;
            }
            else
            {
                // If the children are not loaded, add all the excluded nodes under this parent
                List<NodeId> unselectedNodes = new List<NodeId>();

                foreach (var pair in BlacklistedNodesPaths)
                {
                    if (pair.Value.StartsWith(parent.Node.Path))
                        unselectedNodes.Add(pair.Key);
                    if (unselectedNodes.Count == 0)
                        Logger.Log(Logger.Level.Error, $"Logic error in UnselectedChildNodes: parent node {parent.Node.NodeId} - {parent.Node.Name} is indeterminate but all children are selected.");
                }
                return unselectedNodes;
            }
        }

        private static async void OnSyncDbIdChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SyncExclusionSelector control && control.IsLoaded)
                await control.LoadTreeViewData();
        }

        /// <summary>
        /// Asynchronously loads / refreshes the data displayed in the tree view, applying the current sync exclusion fetch from the server.
        /// </summary>
        /// <returns>A task that represents the asynchronous load operation.</returns>
        private async Task LoadTreeViewData()
        {
            IsLoading = true;
            await RefreshBlackListedNodesAsync();
            await LoadRootItemsAsync();
            IsLoading = false; // Let all the pending check/uncheck propagate
        }

        /// <summary>
        /// Asynchronously loads the root items from the server and populates the RootItems collection.
        /// </summary>
        /// <remarks>This method clears the existing RootItems collection before loading new items from
        /// the server. It also loads the direct children of each root item to ensure correct expand and collapse
        /// behavior in the UI. If the server fails to return root nodes, an error is logged.</remarks>
        /// <returns>A task that represents the asynchronous load operation.</returns>
        public async Task LoadRootItemsAsync()
        {
            StatePropagationInProgress = true;

            RootItems.Clear();
            if (RootItem is not null)
                RootItem.Children.Clear();
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();

            // Get the root nodes from the server
            Node rootNode = new Node(RemoteNodeId, "", -1, "", "", UserDbId, DriveId);
            RootItem = new TreeItem(rootNode, UserDbId, DriveId, null, BlacklistedNodesPaths);
            var fileItem = RootItem.Children.FirstOrDefault(i => i.Type == NodeType.File);
            if (fileItem is not null) RootItem.Children.Remove(fileItem);
            await RootItem.LoadDirectChildren();
            RootItems.AddRange(RootItem.Children);
            StatePropagationInProgress = false;
            HasPendingChanges = false;
        }

        private async Task RefreshBlackListedNodesAsync()
        {
            DbId syncDbId = SyncDbId ?? -1;
            BlacklistedNodeIds.Clear();

            if (syncDbId != -1) // Only load excluded nodes if a valid SyncDbId is provided, for new syncs there isn't any excluded nodes
            {
                var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
                BlacklistedNodeIds = await commService.GetBlacklistedNodeIdList(syncDbId, CancellationToken.None) ?? new();
                await RefreshBlacklistedNodesPaths();
            }
            else
            {
                BlacklistedNodesPaths.Clear();
            }
        }

        /// <summary>
        /// Asynchronously refreshes the list of blacklisted and undecided nodes, updating their associated paths.
        /// </summary>
        /// <remarks>This method retrieves the latest information for all nodes currently marked as
        /// blacklisted or undecided and updates the internal mapping of node identifiers to their paths. The operation
        /// completes when all node information has been loaded and the mapping is updated.</remarks>
        /// <returns>A task that represents the asynchronous refresh operation.</returns>
        private async Task RefreshBlacklistedNodesPaths()
        {
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            async Task loadPath(NodeId nodeId)
            {
                var nodeInfo = await commService.GetNodeInfo(UserDbId, DriveId, nodeId, CancellationToken.None);
                if (nodeId is null || nodeInfo is null)
                {
                    Logger.Log(Logger.Level.Warning, "Failed to load node info for nodeId: " + nodeId);
                    return;
                }
                BlacklistedNodesPaths.Add(nodeId, nodeInfo.Path);
            }

            BlacklistedNodesPaths = BlacklistedNodesPaths.Where((pair) => BlacklistedNodeIds.Contains(pair.Key)).ToDictionary(); // Remove all the nodes that are not blacklisted anymore.

            List<Task> loadExcludedNodePathTasks = new List<Task>();
            foreach (var item in BlacklistedNodeIds.Where((nodeId) => !BlacklistedNodesPaths.ContainsKey(nodeId))) // Fetch the path of all the new nodes
            {
                loadExcludedNodePathTasks.Add(loadPath(item));
            }
            await Task.WhenAll(loadExcludedNodePathTasks);
        }

        /// <summary>
        /// Handles the event that occurs when a TreeView item is expanding, asynchronously loading (if needed) the n-2 children
        /// of the expanding item. this ensures that the expand/collapse icons are displayed correctly in the UI.
        /// </summary>
        /// <remarks>This method ensures that all direct children of the expanding item are loaded before
        /// the expansion completes. It sets a state flag to indicate that state propagation is in progress during the
        /// asynchronous operation.</remarks>
        /// <param name="sender">The TreeView control that raised the expanding event.</param>
        /// <param name="args">The event data containing information about the item being expanded.</param>
        private async void TreeView_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            StatePropagationInProgress = true;
            var item = (args.Item as TreeItem);
            if (item is not null)
                await item.LoadDirectChildren();

            StatePropagationInProgress = false;
        }

        private void CheckBox_Checked(object sender, RoutedEventArgs e)
        {
            var checkbox = sender as CheckBox;
            var treeItem = checkbox?.DataContext as TreeItem;
            if (treeItem is null) treeItem = checkbox?.Tag as TreeItem;
            if (treeItem is not null && !StatePropagationInProgress)
            {
                StatePropagationInProgress = true;
                treeItem.IsSelected = true;
                SelectAllChilds(treeItem);
                StatePropagationInProgress = false;
                if (!IsLoading) SetHasPendingChangesIfNeeded();
            }
        }

        private void SelectAllChilds(TreeItem parent)
        {
            foreach (var child in parent.Children)
            {
                child.IsSelected = true;
                SelectAllChilds(child);
            }
        }

        private void CheckBox_Unchecked(object sender, RoutedEventArgs e)
        {
            var checkbox = sender as CheckBox;
            var treeItem = checkbox?.DataContext as TreeItem;
            if (treeItem is null) treeItem = checkbox?.Tag as TreeItem;
            if (treeItem is not null && !StatePropagationInProgress)
            {
                StatePropagationInProgress = true;
                treeItem.IsSelected = false;
                DeselectAllChilds(treeItem);
                StatePropagationInProgress = false;
                if (!IsLoading) SetHasPendingChangesIfNeeded();
            }
        }

        private void DeselectAllChilds(TreeItem parent)
        {
            foreach (var child in parent.Children.Where((child) => child.Type != NodeType.File))
            {
                child.IsSelected = false;
                DeselectAllChilds(child);
            }

            // Also deselect the file child if it exists
            var fileChild = parent.Children.FirstOrDefault(c => c.Type == NodeType.File);
            if (fileChild is not null)
                fileChild.IsSelected = false;
        }

        private void CheckBox_Indeterminate(object sender, RoutedEventArgs e)
        {
            var checkbox = sender as CheckBox;
            var treeItem = checkbox?.DataContext as TreeItem;
            if (treeItem is null) treeItem = checkbox?.Tag as TreeItem;
            if (treeItem is not null && !StatePropagationInProgress && treeItem.IsSelected is not null)
                // Treat indeterminate as a toggle between checked and unchecked
                if (treeItem.IsSelected.Value)
                    CheckBox_Unchecked(sender, e);
                else
                    CheckBox_Checked(sender, e);
        }

        private async void SizeContentLoader_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            TreeItem? treeItem = (sender as Control)?.DataContext as TreeItem;
            if (treeItem is not null)
                if (treeItem.Node.Size == -1)
                    await treeItem.Node.LoadSize();
        }

        void SetHasPendingChangesIfNeeded()
        {
            if (HasPendingChanges) return;
            if(RootItem is null)
            {
                HasPendingChanges = false;
                return;
            }
            var unselectedNodes = UnselectedChildNodes(RootItem);
            HasPendingChanges = unselectedNodes.Count != BlacklistedNodeIds.Count || unselectedNodes.Except(BlacklistedNodeIds).Any() || BlacklistedNodeIds.Except(unselectedNodes).Any();
        }
    }

    public class TreeItem : UISafeObservableObject
    {
        private DbId _userDbId = 0;
        private DriveId _driveId;
        private bool _isLoadingChildren = false;
        private bool _isEnabled = true;
        private bool? _isSelected = true;
        public Node Node { get; private set; }
        public bool IsLoadingChildren
        {
            get => _isLoadingChildren;
            private set => SetPropertyInUIThread(ref _isLoadingChildren, value);
        }

        public bool ChildrenLoaded { get; private set; } = false;

        public bool IsEnabled
        {
            get => _isEnabled;
            set => SetPropertyInUIThread(ref _isEnabled, value);
        }
        public bool? IsSelected
        {
            get => _isSelected;
            set => SetPropertyInUIThread(ref _isSelected, value);
        }

        public ObservableCollection<TreeItem> Children { get; } = new ObservableCollection<TreeItem>();
        public TreeItem? ParentItem { get; }
        public NodeType Type { get; }
        public Dictionary<NodeId, string> ExcludedNodes { get; }
        public TreeItem(Node node, DbId userDbId, DriveId driveId, TreeItem? parentItem, Dictionary<NodeId, string> excluded, NodeType type = NodeType.Directory)
        {
            Node = node;
            ParentItem = parentItem;
            _userDbId = userDbId;
            _driveId = driveId;
            Type = type;
            ExcludedNodes = excluded;
            if (Type == NodeType.Directory)
            {
                Children.Add(new TreeItem(new Node("-1", "", 0, "", "", _userDbId, _driveId), 0, 0, this, ExcludedNodes, NodeType.File));
                Children.CollectionChanged += OnChildrenCollectionChanged;
                PropertyChanged += OnPropertyChanged;
                RegisterCallbcak(Children);

                refreshFileItemState();
            }
            if (Type == NodeType.File)
            {
                Node.Name = "Fichiers du dossier " + ParentItem?.Node.Name ?? "";
                Node.Path = parentItem?.Node.Path + System.IO.Path.DirectorySeparatorChar + Node.Name;
                ChildrenLoaded = true;
            }
            UpdateSelectedWithExluded();
        }

        public void UpdateSelectedWithExluded()
        {
            if (ParentItem is not null && ParentItem.IsSelected is not null)
            {
                IsSelected = ParentItem.IsSelected;
            }
            else
            {
                if (ExcludedNodes.Any((pair) => pair.Key == Node.NodeId || pair.Value == Node.Path))
                {
                    // This node is in the exluded list
                    IsSelected = false;
                }
                else if (ExcludedNodes.Any((pair) => pair.Value.StartsWith(Node.Path)))
                {
                    // A child of this node is in the excluded list
                    IsSelected = null;
                }
                else
                {
                    IsSelected = true;
                }
            }
            foreach (var child in Children)
            {
                child.UpdateSelectedWithExluded();
            }
        }

        private void OnPropertyChanged(object? sender, PropertyChangedEventArgs e)
        {
            if (e.PropertyName == nameof(TreeItem.IsSelected))
            {
                refreshFileItemState();
            }
        }

        private void OnChildrenCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e)
        {
            RegisterCallbcak(e.NewItems?.Cast<TreeItem>().ToList());
        }

        private void RegisterCallbcak(IEnumerable<TreeItem>? newItems)
        {
            if (newItems != null)
                foreach (var item in newItems)
                    if (item is TreeItem treeItem)
                        treeItem.WhenAnyPropertyChanged(nameof(TreeItem.IsSelected)).Subscribe(item =>
                        {
                            if (item?.Type != NodeType.File) refreshFileItemState();
                            refreshDirectoryItemState();
                        });
            refreshFileItemState();
        }

        void refreshFileItemState()
        {
            if (Type == NodeType.File) return;

            var fileItem = Children.FirstOrDefault(c => c.Type == NodeType.File);
            if (fileItem == null)
                return;


            if (!(IsSelected ?? true))
            {
                fileItem.IsSelected = false;
                fileItem.IsEnabled = true;
                return;
            }

            bool hasSelectedChild = Children.Where(c => c.Type == NodeType.Directory).Any(c => c.IsSelected ?? true);
            if (hasSelectedChild) fileItem.IsSelected = hasSelectedChild;
            fileItem.IsEnabled = !hasSelectedChild;
        }

        void refreshDirectoryItemState()
        {
            if (Type == NodeType.File) return;

            if (Children.All(c => c.IsSelected == true)) // If all children are selected, set parent to selected
            {
                IsSelected = true;
                return;
            }
            else if (Children.All(c => c.IsSelected == false)) // If no children are selected, set to unselected
            {
                IsSelected = false;
                return;
            }
            else // Otherwise, set parent to indeterminate
            {
                IsSelected = null;
            }

        }
        public async Task LoadDirectChildren()
        {
            if (ChildrenLoaded || Type == NodeType.File) return;
            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(_userDbId, _driveId, Node.NodeId, CancellationToken.None);
            List<TreeItem> tmpChildren = new List<TreeItem>();
            if (nodes != null)
            {
                foreach (Node node in nodes)
                    Children.Add(new TreeItem(node, _userDbId, _driveId, this, ExcludedNodes));

                ChildrenLoaded = true;
            }
            else
                Logger.Log(Logger.Level.Error, "Failed to load root nodes for SyncExclusionSelector.");
            IsLoadingChildren = false;
        }
    }

    public class NodeTypeTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? DirectoryTemplate { get; set; }
        public DataTemplate? FileTemplate { get; set; }
        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is TreeItem treeItem)
            {
                return treeItem.Type == NodeType.Directory ? DirectoryTemplate : FileTemplate;
            }
            return base.SelectTemplateCore(item);
        }
    }

}
