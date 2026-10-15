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
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using static Infomaniak.kDrive.ServerCommunication.Interfaces.IServerCommService;

namespace Infomaniak.kDrive.CustomControls
{
    /// <summary>
    /// Runs paginated searches and exposes the results as a list of search box items
    /// (results, loaders and "not found" placeholder).
    /// </summary>
    internal sealed class SearchResultsPager(IServerCommService commService)
    {
        private const int FirstPageLoaderCount = 3;

        private CancellationTokenSource _cts = new();
        private Sync? _sync;
        private string _query = "";
        private string _cursor = "";
        private bool _hasMore = false;
        private bool _isLoadingFirstPage = false;
        private bool _isLoadingNextPage = false;

        public ObservableCollection<ISearchBoxResultItem> Items { get; } = [new SearchBoxNotFoundItem()];

        /// <summary>Raised right before loader items are added or removed.</summary>
        public event EventHandler? LoaderTransitionStarting;

        /// <summary>Starts a new search. Returns false if the search failed.</summary>
        public async Task<bool> SearchAsync(Sync? sync, string query)
        {
            var token = Restart();
            _sync = sync;
            _query = query;

            if (sync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem called but no sync is selected.");
                ShowNotFound();
                return true;
            }

            _isLoadingFirstPage = true;
            LoaderTransitionStarting?.Invoke(this, EventArgs.Empty);
            ReplaceItems(Enumerable.Range(0, FirstPageLoaderCount).Select(_ => new SearchBoxLoaderItem()));

            var page = await FetchPageAsync(cursor: "", token);
            if (page is null)
                return true;

            _isLoadingFirstPage = false;
            if (page.Value.Code != ExitCode.Ok || page.Value.Items.Count == 0)
            {
                ShowNotFound();
                return page.Value.Code == ExitCode.Ok;
            }

            ReplaceItems(ToItems(page.Value));
            return true;
        }

        public void ClearQuery()
        {
            Restart();
            ShowNotFound();
        }

        public async Task LoadNextPageAsync()
        {
            if (_isLoadingFirstPage || _isLoadingNextPage || !_hasMore)
                return;

            _isLoadingNextPage = true;
            var page = await FetchPageAsync(_cursor, _cts.Token);
            if (page is null)
                return;

            _isLoadingNextPage = false;
            LoaderTransitionStarting?.Invoke(this, EventArgs.Empty);
            if (Items.LastOrDefault() is SearchBoxLoaderItem { IsLoadMore: true })
                Items.RemoveAt(Items.Count - 1);

            if (page.Value.Code != ExitCode.Ok)
                return;

            foreach (var item in ToItems(page.Value))
                Items.Add(item);
        }

        // Returns null if the request was cancelled.
        private async Task<SearchItemResult?> FetchPageAsync(string cursor, CancellationToken token)
        {
            try
            {
                var page = await commService.SearchItem(_sync, _query, cursor, token);
                if (token.IsCancellationRequested)
                    return null;

                if (page.Code != ExitCode.Ok)
                    Logger.Log(Logger.Level.Warning, $"SearchItem failed with cause {page.Code}.", "SearchResultsPager: SearchItem failed.");

                _cursor = page.Cursor;
                _hasMore = page.Code == ExitCode.Ok && page.HasMore;
                return page;
            }
            catch (OperationCanceledException)
            {
                return null;
            }
        }

        private CancellationToken Restart()
        {
            _cts.Cancel();
            _cts.Dispose();
            _cts = new CancellationTokenSource();

            _cursor = "";
            _hasMore = false;
            _isLoadingFirstPage = false;
            _isLoadingNextPage = false;
            return _cts.Token;
        }

        private void ShowNotFound()
        {
            _cursor = "";
            _hasMore = false;
            ReplaceItems([new SearchBoxNotFoundItem()]);
        }

        private IEnumerable<ISearchBoxResultItem> ToItems(SearchItemResult page)
        {
            foreach (var searchItem in page.Items)
                yield return new SearchBoxResultItem(searchItem);
            if (_hasMore)
                yield return new SearchBoxLoaderItem(IsLoadMore: true);
        }

        // Updates in place to avoid collapsing the popup and triggering add/remove animations.
        private void ReplaceItems(IEnumerable<ISearchBoxResultItem> items)
        {
            var newItems = items.ToList();
            for (int i = 0; i < newItems.Count; i++)
            {
                if (i >= Items.Count)
                    Items.Add(newItems[i]);
                else if (!ReferenceEquals(Items[i], newItems[i]))
                    Items[i] = newItems[i];
            }
            while (Items.Count > newItems.Count)
                Items.RemoveAt(Items.Count - 1);
        }
    }
}
