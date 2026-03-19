using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
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
        private readonly ObservableCollection<RemoteLocationSelectorTreeItem> _rootLevelItems = [];
        private RemoteLocationSelectorTreeItem? _currentFolderCreationItem;
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

        public IDrive? Drive
        {
            get => (IDrive?)GetValue(DriveProperty);
            set => SetValue(DriveProperty, value);
        }
        public static readonly DependencyProperty DriveProperty =
            DependencyProperty.Register(nameof(Drive), typeof(IDrive), typeof(RemoteLocationSelector), new PropertyMetadata(null));

        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }
        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(RemoteLocationSelector), new PropertyMetadata(true));

        public bool HasSelectedNode
        {
            get => (bool)GetValue(HasSelectedNodeProperty);
            set => SetValue(HasSelectedNodeProperty, value);
        }

        public static readonly DependencyProperty HasSelectedNodeProperty =
            DependencyProperty.Register(nameof(HasSelectedNode), typeof(bool), typeof(RemoteLocationSelector), new PropertyMetadata(false));
        #endregion

        #region Public methods
        public NodeId? GetSelectedNodeId()
        {
            if (FolderTree.SelectedItem is RemoteLocationSelectorTreeItem selectedItem)
                return selectedItem.Node?.NodeId;
            return null;
        }
        #endregion

        #region Loading / refreshing
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
            foreach (var item in _rootLevelItems)
            {
                item.Dispose();
            }
            _rootLevelItems.Clear();
        }

        public async Task BuildRootLevelItemsAsync()
        {
            _rootLevelItems.Clear();

            if (Drive is null)
            {
                Logger.Log(Logger.Level.Error, "Drive is null in BuildRootLevelItemsAsync.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            // Logical root node
            Node rootNode = new Node(App.Constants.Drive.RootNodeId, Drive.Name, -1, "", "", Drive.UserDbId, Drive.DriveId, false);
            _rootLevelItems.Add(new RemoteLocationSelectorTreeItem(rootNode, Drive, null));
            await _rootLevelItems[0].LoadImmediateChildrenAsync();
            await rootNode.LoadSize();
        }
        #endregion

        #region TreeView events
        private async void TreeView_Expanding(TreeView sender, TreeViewExpandingEventArgs args)
        {
            List<Task> tasks = [];
            if (args.Item is RemoteLocationSelectorTreeItem item)
            {
                foreach (var child in item.Children.Where(i => i.Node is not null))
                {
                    tasks.Add(child.LoadImmediateChildrenAsync());
                }
                await Task.WhenAll(tasks);
            }
        }
        #endregion

        #region Misc UI helpers

        // Lazy load size
        private async void SizeContentLoader_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if ((sender as Control)?.DataContext is RemoteLocationSelectorTreeItem treeItem && treeItem.Node is not null && treeItem.Node.Size == -1)
                await treeItem.Node.LoadSize();
        }
        #endregion

        private void FolderTree_SelectionChanged(TreeView sender, TreeViewSelectionChangedEventArgs args)
        {
            RemoteLocationSelectorTreeItem? selectedItem = args.AddedItems.FirstOrDefault() as RemoteLocationSelectorTreeItem;
            RemoteLocationSelectorTreeItem? previousItem = args.RemovedItems.FirstOrDefault() as RemoteLocationSelectorTreeItem;

            if (selectedItem is null)
            {
                HasSelectedNode = false;
                return;
            }

            // If the selected item is not selectable (e.g. access denied, tmp node), revert the selection to the previous item
            if (selectedItem.Node is null || selectedItem.Node.AccessDenied || selectedItem.ParentItem is null)
                DispatcherQueue.TryEnqueue(() =>
                {
                    HasSelectedNode = previousItem is not null;
                    sender.SelectedItem = previousItem;
                });
            else
                HasSelectedNode = true;
        }

        private void RemoveCurrentFolderCreationItem()
        {
            if (_currentFolderCreationItem is not null && _currentFolderCreationItem.ParentItem is not null)
            {
                _currentFolderCreationItem.ParentItem.Children.Remove(_currentFolderCreationItem);
                _currentFolderCreationItem.ParentItem.CanCreateSubFolder = true;
                _currentFolderCreationItem = null;
            }
        }

        private async void CreateFolderButton_Click(object sender, RoutedEventArgs e)
        {
            Control? control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Error, "CreateFolderButton_Click: sender is not a Control.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (control.DataContext is not RemoteLocationSelectorTreeItem treeItem)
            {
                Logger.Log(Logger.Level.Error, "CreateFolderButton_Click: DataContext is not a TreeItem2.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }
            RemoveCurrentFolderCreationItem();
            treeItem.CanCreateSubFolder = false;
            // Create a temporary TreeItem with no Node, it will allow the user to enter the name of the new folder.
            // Once the name is entered, the real TreeItem with the Node will be created and replace the temporary one.
            RemoteLocationSelectorTreeItem newItem = new RemoteLocationSelectorTreeItem(Drive!, treeItem);
            treeItem.Children.Insert(0, newItem);
            _currentFolderCreationItem = newItem;
            // Expand the parent item to make the new item visible and get its container
            TreeViewNode? node = FindNode(FolderTree.RootNodes, treeItem);
            if (node is not null)
                node.IsExpanded = true;

            TreeViewNode? newItemNode = FindNode(FolderTree.RootNodes, newItem);
            if (newItemNode is null)
                return;

            var container = await WaitForContainerAsync(newItemNode);
            if (container is null)
                return;

            // Set the focus on the TextBox in the new item's template to allow the user to enter the folder name easily
            var textBox = FindChild(container, typeof(TextBox)) as TextBox;
            if (textBox is not null)
            {
                textBox.Focus(FocusState.Programmatic);
                textBox.SelectAll();
            }
        }

        private Task<TreeViewItem?> WaitForContainerAsync(TreeViewNode node)
        {
            var tcs = new TaskCompletionSource<TreeViewItem?>();
            var deadline = DateTime.UtcNow.AddSeconds(3);

            void OnLayoutUpdated(object? s, object e)
            {
                var container = FolderTree.ContainerFromNode(node) as TreeViewItem;
                if (container is not null || DateTime.UtcNow >= deadline)
                {
                    FolderTree.LayoutUpdated -= OnLayoutUpdated;
                    tcs.TrySetResult(container);
                }
            }

            FolderTree.LayoutUpdated += OnLayoutUpdated;
            return tcs.Task;
        }

        private TreeViewNode? FindNode(IList<TreeViewNode> nodes, RemoteLocationSelectorTreeItem target)
        {
            foreach (var node in nodes)
            {
                if (node.Content == target)
                    return node;

                var found = FindNode(node.Children, target);
                if (found != null)
                    return found;
            }
            return null;
        }

        private async void NewItemTextBox_KeyDown(object sender, Microsoft.UI.Xaml.Input.KeyRoutedEventArgs e)
        {
            TextBox? textBox = sender as TextBox;
            if (textBox is null)
            {
                Logger.Log(Logger.Level.Error, "NewItemTextBox_KeyDown: sender is not a TextBox.");
                Utility.ShowUnexpectedErrorTeachingTip();
                return;
            }

            if (e.Key == Windows.System.VirtualKey.Enter)
            {
                e.Handled = true;

                var contentLoader = FindParent(textBox, typeof(ContentLoader)) as ContentLoader;
                if (contentLoader is not null)
                    contentLoader.IsLoading = true;

                var treeItem = textBox.DataContext as RemoteLocationSelectorTreeItem;
                var parentTreeItem = treeItem?.ParentItem;
                if (treeItem is null || parentTreeItem is null)
                {
                    Logger.Log(Logger.Level.Error, "treeItem or parentTreeItem is null");
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                var newItem = await CreateFolder(parentTreeItem, textBox.Text.Trim());

                if (newItem is null)
                {
                    Utility.ShowTeachingTipFromKeys("unableToCreateRemoteFolderTeachingTipTitle", "", "unableToCreateRemoteFolderTeachingTipContent", TimeSpan.FromSeconds(20));
                    if (contentLoader is not null)
                        contentLoader.IsLoading = false;
                    return;
                }

                if (contentLoader is not null)
                    contentLoader.IsLoading = false;

                RemoveCurrentFolderCreationItem();
                FolderTree.SelectedItem = newItem;
            }
            else if (e.Key == Windows.System.VirtualKey.Escape)
            {
                e.Handled = true;
                RemoveCurrentFolderCreationItem();
            }

        }

        private async Task<RemoteLocationSelectorTreeItem?> CreateFolder(RemoteLocationSelectorTreeItem parentItem, string folderName)
        {
            if (parentItem.Node is null)
            {
                Logger.Log(Logger.Level.Error, "Parent node is null.");
                return null;
            }
            var parentNodeId = parentItem.Node.NodeId ?? "";

            if (string.IsNullOrEmpty(parentNodeId))
            {
                parentNodeId = App.Constants.Drive.RootNodeId;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            NodeId? newNodeId = await commService.CreateMissingDirectories(Drive!, parentNodeId, folderName, CancellationToken.None);
            if (newNodeId is null)
            {
                Logger.Log(Logger.Level.Error, "Failed to create folder.");
                return null;
            }

            // Add the node to the tree
            var newTreeItem = new RemoteLocationSelectorTreeItem(new Node(newNodeId, folderName, 0, parentNodeId, System.IO.Path.Combine(parentItem.Node?.Path ?? "", folderName), Drive!.UserDbId, Drive.DriveId, false), Drive!, parentItem);
            parentItem.Children.Insert(0, newTreeItem);
            return newTreeItem;
        }

        private static object? FindParent(DependencyObject dependencyObject, Type ancestorType)
        {
            DependencyObject parent = VisualTreeHelper.GetParent(dependencyObject);
            if (parent is null)
                return null;

            if (ancestorType.IsAssignableFrom(parent.GetType()))
                return parent;

            return FindParent(parent, ancestorType);
        }

        private static object? FindChild(DependencyObject parent, Type childType)
        {
            if (parent is null)
                return null;

            int count = VisualTreeHelper.GetChildrenCount(parent);

            for (int i = 0; i < count; i++)
            {
                DependencyObject child = VisualTreeHelper.GetChild(parent, i);

                if (childType.IsAssignableFrom(child.GetType()))
                    return child;

                object? result = FindChild(child, childType);
                if (result is not null)
                    return result;
            }

            return null;
        }

        private void TreeViewItem_PointerExited(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            RemoteLocationSelectorTreeItem? treeItem = (sender as FrameworkElement)?.DataContext as RemoteLocationSelectorTreeItem;
            if (treeItem is not null)
            {
                treeItem.CreateSubFolderButtonVisibility = Visibility.Collapsed;
            }

        }

        private void TreeViewItem_PointerEntered(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            RemoteLocationSelectorTreeItem? treeItem = (sender as FrameworkElement)?.DataContext as RemoteLocationSelectorTreeItem;
            if (treeItem is not null && treeItem.Node is not null)
            {
                treeItem.CreateSubFolderButtonVisibility = Visibility.Visible;
            }
        }

        private void NewItemTextBox_LostFocus(object sender, RoutedEventArgs e)
        {
            TextBox? textBox = sender as TextBox;
            if (textBox is not null && textBox.Text.Length == 0)
                RemoveCurrentFolderCreationItem();
        }
    }

    public class RemoteLocationSelectorTreeItem : UISafeObservableObject, IDisposable
    {
        #region Private fields
        private readonly IDrive _drive;
        private bool _isSelected = false;
        private bool _isLoadingChildren = false;
        private bool _childrenLoaded = false;
        private bool _canCreateSubfolder = false;
        private Visibility _createSubFolderButtonVisibility = Visibility.Collapsed;
        private bool _disposed = false;
        #endregion

        #region Public properties
        public Node? Node { get; private set; }

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

        public bool CanCreateSubFolder
        {
            get => _canCreateSubfolder;
            set => SetPropertyInUIThread(ref _canCreateSubfolder, value);
        }

        public Visibility CreateSubFolderButtonVisibility
        {
            get => _createSubFolderButtonVisibility;
            set => SetPropertyInUIThread(ref _createSubFolderButtonVisibility, value);
        }

        public IDrive Drive => _drive;

        public ObservableCollection<RemoteLocationSelectorTreeItem> Children { get; } = [];
        public RemoteLocationSelectorTreeItem? ParentItem { get; }
        public bool IsRoot => ParentItem is null;
        public bool IsNotRoot => ParentItem is not null;
        #endregion

        public RemoteLocationSelectorTreeItem(Node node, IDrive drive, RemoteLocationSelectorTreeItem? parentItem)
        {
            Node = node;
            ParentItem = parentItem;
            _drive = drive;
            _canCreateSubfolder = true;
        }

        // Constructor used for tmp nodes (the user is creating a new folder)
        public RemoteLocationSelectorTreeItem(IDrive drive, RemoteLocationSelectorTreeItem? parentItem)
        {
            ParentItem = parentItem;
            _canCreateSubfolder = false;
            _drive = drive;
        }

        // Lazy load direct child directories
        public async Task LoadImmediateChildrenAsync()
        {
            if (_childrenLoaded || Node is null || Node.AccessDenied)
                return;
            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(_drive.UserDbId, _drive.DriveId, Node.NodeId, CancellationToken.None);
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
                Children.Add(new RemoteLocationSelectorTreeItem(node, _drive, this));
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

    public partial class RemoteLocationSelectorTreeViewTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FolderTemplate { get; set; }
        public DataTemplate? AccessDeniedTemplate { get; set; }
        public DataTemplate? NewNodeTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is not RemoteLocationSelectorTreeItem treeItem)
                return base.SelectTemplateCore(item);

            if (treeItem.Node is null)
                return NewNodeTemplate;

            return treeItem.Node.AccessDenied ? AccessDeniedTemplate : FolderTemplate;
        }
    }
}
