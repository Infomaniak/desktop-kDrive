/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /// <summary>
    /// Non-generic base for lazy-loaded tree items. It exposes the members the shared
    /// <see cref="FolderTreeSelectorBase"/> control needs to manipulate items without knowing
    /// their concrete type (loading state, the loadable node and the lazy child loading entry point).
    /// </summary>
    public abstract class LazyLoadedTreeItemBase : UISafeObservableObject, System.IDisposable
    {
        #region Private fields
        private bool _isLoadingChildren = false;
        private bool _childrenLoaded = false;
        private bool _disposed = false;
        #endregion

        #region Public properties
        public bool IsLoadingChildren
        {
            get => _isLoadingChildren;
            protected set => SetPropertyInUIThread(ref _isLoadingChildren, value);
        }

        public bool ChildrenLoaded
        {
            get => _childrenLoaded;
            protected set => SetPropertyInUIThread(ref _childrenLoaded, value);
        }

        private Node? _node;
        private bool? _isSelected;

        // Node whose sub folders / size should be loaded, or null when this item has no real node yet
        // (e.g. a temporary item created while the user types a new folder name).
        public Node? Node
        {
            get => _node;
            protected set => SetPropertyInUIThread(ref _node, value);
        }

        public virtual bool? IsSelected
        {
            get => _isSelected;
            set => SetPropertyInUIThread(ref _isSelected, value);
        }

        public virtual Node? LoadableNode => Node;
        #endregion

        // Lazy load direct child directories.
        public abstract Task LoadImmediateChildrenAsync(CancellationToken cancellationToken);

        public void Dispose()
        {
            if (_disposed)
                return;

            _disposed = true;

            DisposeManagedResources();
            DisposeChildren();
        }

        // Recursively dispose the typed children collection owned by the derived class.
        protected abstract void DisposeChildren();

        // Hook letting derived items release their own resources before children are disposed.
        protected virtual void DisposeManagedResources()
        {
        }
    }

    /// <summary>
    /// Base class shared by the tree items of <see cref="SyncExclusionSelector"/> and
    /// <see cref="RemoteLocationSelector"/>. It centralizes the concurrency-safe, cancellable
    /// lazy loading of direct child directories as well as the recursive disposal logic.
    /// Concrete items only have to expose their drive context and provide a factory for children.
    /// </summary>
    public abstract class LazyLoadedTreeItem<TSelf> : LazyLoadedTreeItemBase
        where TSelf : LazyLoadedTreeItem<TSelf>
    {
        #region Public properties
        public ObservableCollection<TSelf> Children { get; } = [];
        public TSelf? ParentItem { get; }
        #endregion

        protected LazyLoadedTreeItem(TSelf? parentItem)
        {
            ParentItem = parentItem;
        }

        #region Abstract members
        // Drive context used to fetch the children from the server.
        protected abstract DbId UserDbId { get; }
        protected abstract DriveId DriveId { get; }

        // Factory creating a concrete child item for the given node.
        protected abstract TSelf CreateChild(Node node);
        #endregion

        // Lazy load direct child directories
        public override async Task LoadImmediateChildrenAsync(CancellationToken cancellationToken)
        {
            Node? node = LoadableNode;
            if (ChildrenLoaded || node is null || node.AccessDenied || IsLoadingChildren)
                return;

            IsLoadingChildren = true;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            List<Node>? nodes = await commService.GetSubFolders(UserDbId, DriveId, node.NodeId, cancellationToken);
            if (nodes is null)
            {
                if (!cancellationToken.IsCancellationRequested)
                {
                    Logger.Log(Logger.Level.Error, "Failed to load child folders.");
                    Utility.ShowUnexpectedErrorTeachingTip();
                }
                ChildrenLoaded = true;
                IsLoadingChildren = false;
                return;
            }

            foreach (Node child in nodes)
                await Utility.RunOnUIThread(() => Children.Add(CreateChild(child)));
            ChildrenLoaded = true;
            IsLoadingChildren = false;
        }

        protected override void DisposeChildren()
        {
            foreach (var child in Children)
            {
                child.Dispose();
            }
            Children.Clear();
        }
    }
}
