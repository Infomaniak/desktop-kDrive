using DynamicData;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{

    public sealed partial class RemoteLocationSelector : UserControl
    {
        #region Private fields

        // Root level items displayed in the TreeView, it should only contain the drive itself
        private readonly ObservableCollection<TreeItem2> _rootLevelItems = [];

        #endregion

        #region Constructor / lifecycle
        public RemoteLocationSelector()
        {
            InitializeComponent();
        }

        // Initial load of data once control is displayed
        private async void RemoteLocationSelector_Loaded(object sender, RoutedEventArgs e)
        {
            await ReloadAsync();
        }

        private void RemoteLocationSelector_Unloaded(object sender, RoutedEventArgs e)
        {
            ClearAllTreeItems();
        }
        #endregion

        #region Dependency Properties

        public IDrive Drive
        {
            get => (IDrive)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }
        public static readonly DependencyProperty DriveProperty =
            DependencyProperty.Register(nameof(Drive), typeof(IDrive), typeof(SyncExclusionSelector), new PropertyMetadata(null));

        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }
        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(SyncExclusionSelector), new PropertyMetadata(true));


        // Internal root item
        private TreeItem2 RootTreeItem
        {
            get => (TreeItem2)GetValue(RootTreeItemProperty);
            set => SetValue(RootTreeItemProperty, value);
        }
        private static readonly DependencyProperty RootTreeItemProperty =
            DependencyProperty.Register(nameof(RootTreeItem), typeof(TreeItem2), typeof(SyncExclusionSelector), new PropertyMetadata(null));
        #endregion

        #region Public methods


        #endregion

        #region Loading / refreshing
        // Reload full state: exclusion map + tree items
        private async Task ReloadAsync()
        {
            IsLoading = true;
            ClearAllTreeItems();
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

            if (RootTreeItem is not null)
            {
                RootTreeItem.Dispose();
                RootTreeItem = null;
            }
            _rootLevelItems.Clear();

            // Logical root node
            Node rootNode = new Node("", Drive.Name, -1, "", "", Drive.UserDbId, Drive.DriveId, false);
            RootTreeItem = new TreeItem2(rootNode, Drive.UserDbId, Drive, null);
            _rootLevelItems.Add(RootTreeItem);
            await RootTreeItem.LoadImmediateChildrenAsync();
        }

        #endregion

        #region TreeView events
        private async void TreeView_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            List<Task> tasks = [];
            if (args.Item is TreeItem2 item)
            {
                foreach (var child in item.Children)
                {
                    tasks.Add(child.LoadImmediateChildrenAsync());
                }
                await Task.WhenAll(tasks);
            }
        }
        #endregion

        #region Selection propagation helpers
        private static TreeItem? ExtractTreeItemFromSender(object sender)
        {
            var control = sender as Control;
            return control?.DataContext as TreeItem;
        }


        #endregion

        #region Misc UI helpers

        // Lazy load size
        private async void SizeContentLoader_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if ((sender as Control)?.DataContext is TreeItem2 treeItem && treeItem.Node.Size == -1)
                await treeItem.Node.LoadSize();
        }
        #endregion

        private void FolderTree_SelectionChanged(TreeView sender, TreeViewSelectionChangedEventArgs args)
        {

            TreeItem2? selectedItem = args.AddedItems.FirstOrDefault() as TreeItem2;
            TreeItem2? previousItem = args.RemovedItems.FirstOrDefault() as TreeItem2;

            if (selectedItem is null)
            {
                return;
            }

            // If the selected item is access denied or the root of the drive, we revert the selection to the previously selected item
            if (selectedItem.Node.AccessDenied || selectedItem.ParentItem is null)
            {
                DispatcherQueue.TryEnqueue(() => sender.SelectedItem = previousItem);
            }

        }

        private async void CreateFolderButton_Click(object sender, RoutedEventArgs e)
        {
            //         public async Task<NodeId?> CreateMissingDirectories(IDrive drive, NodeId parentNodeId, string path, CancellationToken cancellationToken)

            Control? control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "CreateFolderButton_Click: sender is not a Control.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (control.DataContext is not TreeItem2 treeItem)
            {
                Logger.Log(Logger.Level.Error, "CreateFolderButton_Click: DataContext is not a TreeItem2.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            // Here, we should add a tmp item just under this one in the tree view.
            // the tmp item should have a textbox to enter the name of the new folder, and a confirm button to validate the creation.
            // Once the name is entered and the confirm button is clicked, we should call the CreateMissingDirectories method to create the folder on the server, then replace the tmp item with the new folder item if the creation is successful, or show an error message if it fails.            
        }
    }

    /// <summary>
    /// TreeItem wraps a Node and adds UI-only state: tri-state selection plus lazy child loading logic.
    /// </summary>
    public class TreeItem2 : UISafeObservableObject, IDisposable
    {
        #region Private fields
        private readonly DbId _userDbId = 0;
        private readonly IDrive _drive;
        private bool _isSelected = false;
        private bool _isLoadingChildren = false;
        private bool _childrenLoaded = false;
        private bool _disposed = false;
        #endregion

        #region Public properties
        public Node Node { get; private set; }

        public bool IsSelected
        {
            get => _isSelected;
            set => SetPropertyInUIThread(ref _isSelected, value);
        }

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

        public IDrive Drive => _drive;

        public ObservableCollection<TreeItem2> Children { get; } = [];
        public TreeItem2? ParentItem { get; }
        public bool IsRoot => ParentItem is null;
        public bool IsNotRoot => ParentItem is not null;
        #endregion

        public TreeItem2(Node node, DbId userDbId, IDrive drive, TreeItem2? parentItem)
        {
            Node = node;
            ParentItem = parentItem;
            _userDbId = userDbId;
            _drive = drive;
        }

        // Lazy load direct child directories
        public async Task LoadImmediateChildrenAsync()
        {
            if (_childrenLoaded)
                return;
            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(_userDbId, _drive.DriveId, Node.NodeId, CancellationToken.None);
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
                Children.Add(new TreeItem2(node, _userDbId, _drive, this));
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
            Children.Clear();
        }
    }

    public partial class FolderTreeViewItemTemplateSelector2 : DataTemplateSelector
    {
        public DataTemplate? FolderTemplate { get; set; }
        public DataTemplate? AccessDeniedTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is not TreeItem2 treeItem)
                return base.SelectTemplateCore(item);

            return treeItem.Node.AccessDenied ? AccessDeniedTemplate : FolderTemplate;
        }
    }
}
