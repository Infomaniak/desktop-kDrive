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
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    /// <summary>
    /// Shared base for the controls that display a lazily loaded remote folder tree
    /// (<see cref="SyncExclusionSelector"/> and <see cref="RemoteLocationSelector"/>).
    ///
    /// It mutualizes everything that is not specific to a given selector:
    ///  - the <see cref="CancellationTokenSource"/> lifecycle and <see cref="IAsyncDisposable"/> support,
    ///  - the <see cref="IsLoading"/> dependency property,
    ///  - the <see cref="ReloadAsync"/> orchestration (with a hook for extra data refresh),
    ///  - the mouse-wheel passthrough that lets a parent scroll viewer take over when needed,
    ///  - the viewport-driven lazy loading of children and folder size.
    ///
    /// Derived controls only provide their <see cref="FolderTreeView"/>, how they clear their
    /// (strongly typed) items and how they build the root level items.
    /// </summary>
    public class FolderTreeSelectorBase : UserControl, IAsyncDisposable
    {
        #region Private fields
        private bool _disposed = false;
        #endregion

        #region Protected state
        // Shared cancellation source, reset on every reload and cancelled on disposal.
        protected CancellationTokenSource CancellationTokenSource { get; private set; } = new CancellationTokenSource();

        protected bool IsDisposed => _disposed;
        #endregion

        #region Dependency Properties
        public bool IsLoading
        {
            get => (bool)GetValue(IsLoadingProperty);
            set => SetValue(IsLoadingProperty, value);
        }
        public static readonly DependencyProperty IsLoadingProperty =
            DependencyProperty.Register(nameof(IsLoading), typeof(bool), typeof(FolderTreeSelectorBase), new PropertyMetadata(true));
        #endregion

        #region Virtual members
        // The TreeView hosting the folder hierarchy (the x:Name="FolderTree" of each derived XAML).
        protected virtual TreeView FolderTreeView => throw new NotImplementedException("FolderTreeView must be overridden by derived selectors.");

        // Clear and dispose the (strongly typed) root level items of the derived control.
        protected virtual void ClearAllTreeItems() => throw new NotImplementedException("ClearAllTreeItems must be overridden by derived selectors.");

        // (Re)build the root level items using the current state.
        protected virtual Task BuildRootLevelItemsAsync() => throw new NotImplementedException("BuildRootLevelItemsAsync must be overridden by derived selectors.");
        #endregion

        #region Virtual hooks
        // Optional data refresh performed after the tree is cleared but before items are rebuilt.
        // Return false to abort the reload (the caller will stop and reset IsLoading).
        protected virtual Task<bool> RefreshDataBeforeBuildAsync() => Task.FromResult(true);
        #endregion

        #region Lifecycle
        public FolderTreeSelectorBase()
        {
            Loaded += FolderTreeSelectorBase_Loaded;
        }

        // Lets a derived control opt into the mouse-wheel passthrough once its template is loaded.
        protected void EnableTreeScrollChaining()
        {
            FolderTreeView.AddHandler(
                UIElement.PointerWheelChangedEvent,
                new PointerEventHandler(FolderTree_PointerWheelChanged),
                true);
        }

        protected async void FolderTreeSelectorBase_Loaded(object sender, RoutedEventArgs e)
        {
            await ReloadAsync();
        }

        // Reload full state: reset cancellation, clear items, optional data refresh, rebuild.
        protected async Task ReloadAsync()
        {
            if (_disposed) return;
            IsLoading = true;
            ResetCancellationToken();
            ClearAllTreeItems();
            if (!await RefreshDataBeforeBuildAsync())
            {
                Utility.ShowUnexpectedErrorTeachingTip();
                IsLoading = false;
                return;
            }
            await BuildRootLevelItemsAsync();
            IsLoading = false;
        }

        private void ResetCancellationToken()
        {
            CancellationTokenSource.Cancel();
            if (CancellationTokenSource.TryReset())
            {
                Logger.Log(Logger.Level.Debug, "CancellationTokenSource reset successfully on ReloadAsync.");
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "Failed to reset CancellationTokenSource on reload. A new instance will be created.");
                CancellationTokenSource = new CancellationTokenSource();
            }
        }

        public async ValueTask DisposeAsync()
        {
            if (!_disposed)
            {
                _disposed = true;
                await CancellationTokenSource.CancelAsync();
                CancellationTokenSource.Dispose();
                ClearAllTreeItems();
            }
        }
        #endregion

        #region Mouse wheel passthrough
        private void FolderTree_PointerWheelChanged(object sender, PointerRoutedEventArgs e)
        {
            if (_disposed)
                return;

            var scrollViewer = FindDescendant<ScrollViewer>(FolderTreeView);
            if (scrollViewer == null)
                return;

            int delta = e.GetCurrentPoint(FolderTreeView).Properties.MouseWheelDelta;

            bool scrollingUp = delta > 0;
            bool scrollingDown = delta < 0;

            bool canScrollUp = scrollViewer.VerticalOffset > 0;
            bool canScrollDown = scrollViewer.VerticalOffset < scrollViewer.ScrollableHeight;

            // If the tree can still scroll in the requested direction,
            // consume the event so the parent ScrollViewer does not scroll.
            if ((scrollingUp && !canScrollUp) ||
                (scrollingDown && !canScrollDown))
            {
                e.Handled = false;
            }

            // Otherwise do nothing and let the event bubble to the parent.
        }

        public static T? FindDescendant<T>(DependencyObject root)
            where T : DependencyObject
        {
            if (root is T match)
                return match;

            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(root); i++)
            {
                var child = VisualTreeHelper.GetChild(root, i);

                var result = FindDescendant<T>(child);
                if (result != null)
                    return result;
            }

            return null;
        }
        #endregion

        #region Viewport-driven lazy loading
        // Lazy load children + size when the item is realized, deferring to the viewport handler
        // when the container does not have a measured size yet.
        protected async void TreeViewItem_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if (_disposed) return;

            var control = sender as Control;
            var treeItem = control?.DataContext as LazyLoadedTreeItemBase;
            if (control is null || treeItem is null || treeItem.LoadableNode is null)
                return;

            control.EffectiveViewportChanged -= TreeViewItem_EffectiveViewportChanged;

            if (treeItem.LoadableNode.Size != -1)
            {
                // Size already loaded, no need to load again
                return;
            }

            if (control.ActualHeight > 0 && control.ActualWidth > 0)
            {
                await LoadChildrenAndSizeAsync(treeItem);
            }
            else
            {
                control.EffectiveViewportChanged += TreeViewItem_EffectiveViewportChanged;
            }
        }

        protected async void TreeViewItem_EffectiveViewportChanged(FrameworkElement sender, EffectiveViewportChangedEventArgs args)
        {
            if (_disposed) return;

            var control = sender as Control;
            var treeItem = control?.DataContext as LazyLoadedTreeItemBase;
            if (control is null || treeItem is null || treeItem.LoadableNode is null)
                return;

            if (treeItem.LoadableNode.Size != -1)
            {
                // Size already loaded, no need to load again
                return;
            }

            if (args.EffectiveViewport == Windows.Foundation.Rect.Empty)
                return;

            Windows.Foundation.Rect viewport = args.EffectiveViewport;
            Windows.Foundation.Rect itemRect = new Windows.Foundation.Rect(0, 0, sender.ActualWidth, sender.ActualHeight);

            bool intersects =
                viewport.X < itemRect.X + itemRect.Width &&
                viewport.X + viewport.Width > itemRect.X &&
                viewport.Y < itemRect.Y + itemRect.Height &&
                viewport.Y + viewport.Height > itemRect.Y;

            bool isVisible =
                viewport.Width > 0 &&
                viewport.Height > 0 &&
                intersects;

            if (!isVisible)
                return;

            if (CancellationTokenSource.IsCancellationRequested)
            {
                Logger.Log(Logger.Level.Debug, "TreeViewItem_EffectiveViewportChanged: Cancellation requested, skipping size load.");
                return;
            }

            control.EffectiveViewportChanged -= TreeViewItem_EffectiveViewportChanged;
            await LoadChildrenAndSizeAsync(treeItem);
        }

        private async Task LoadChildrenAndSizeAsync(LazyLoadedTreeItemBase treeItem)
        {
            var loadChildrenTask = treeItem.LoadImmediateChildrenAsync(CancellationTokenSource.Token);
            var loadSizeTask = treeItem.LoadableNode!.LoadSize(CancellationTokenSource.Token);
            await Task.WhenAll(loadChildrenTask, loadSizeTask);
        }
        #endregion
    }
}
