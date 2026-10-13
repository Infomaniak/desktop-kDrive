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
using CommunityToolkit.WinUI;
using Infomaniak.kDrive.Analytics;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Threading;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls
{
    public interface ISearchBoxResultItem
    {
        SearchItem? SearchItem { get; }
        bool IsSelectable { get; }
        bool ShowUnavailableState { get; }
    }

    public sealed class SearchBoxResultItem : UISafeObservableObject, ISearchBoxResultItem
    {
        private bool _showUnavailableState = false;
        public SearchBoxResultItem(SearchItem? SearchItem, bool IsSelectable = true)
        {
            this.SearchItem = SearchItem;
            this.IsSelectable = IsSelectable;
        }

        public SearchItem? SearchItem { get; }
        public bool IsSelectable { get; }
        public bool ShowUnavailableState
        {
            get => _showUnavailableState;
            set => SetPropertyInUIThread(ref _showUnavailableState, value);
        }
    }

    public sealed record SearchBoxLoaderItem(bool IsLoadMore = false) : ISearchBoxResultItem
    {
        public SearchItem? SearchItem => null;
        public bool IsSelectable => false;
        public bool ShowUnavailableState => false;
    };

    public sealed record SearchBoxNotFoundItem() : ISearchBoxResultItem
    {
        public SearchItem? SearchItem => null;
        public bool IsSelectable => false;
        public bool ShowUnavailableState => false;
    };

    public sealed partial class SearchBox : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private CancellationTokenSource? _searchCts;
        private AppModel ViewModel => App.ServiceProvider.GetRequiredService<AppModel>();
        private ISearchBoxResultItem? _lastItemOver;
        private readonly ObservableCollection<ISearchBoxResultItem> _results = [new SearchBoxNotFoundItem()];
        private string _currentQuery = "";
        private string _nextCursor = "";
        private bool _isInitialLoading = false;
        private bool _isLoadingMore = false;

        public SearchBox()
        {
            InitializeComponent();
            TitleBarSearchBox.ItemsSource = _results;
            GotFocus += (_, _) => TitleBarSearchBox.IsSuggestionListOpen = true;
        }

        private void SetResults(IEnumerable<ISearchBoxResultItem> items)
        {
            _results.Clear();
            foreach (var item in items)
                _results.Add(item);
        }

        private void ShowNotFound()
        {
            _nextCursor = "";
            SetResults([new SearchBoxNotFoundItem()]);
        }

        private CancellationToken ResetSearchCancellation()
        {
            if (_searchCts is not null)
            {
                _searchCts.Cancel();
                _searchCts.Dispose();
            }
            _searchCts = new CancellationTokenSource();
            return _searchCts.Token;
        }

        private async void SearchBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {
            if (args.Reason != AutoSuggestionBoxTextChangeReason.UserInput)
                return;

            if (string.IsNullOrWhiteSpace(sender.Text))
            {
                // Keep the last results, unless the initial search was still loading.
                if (_isInitialLoading)
                {
                    ResetSearchCancellation();
                    _isInitialLoading = false;
                    _isLoadingMore = false;
                    ShowNotFound();
                }
                return;
            }

            var token = ResetSearchCancellation();
            _currentQuery = sender.Text;
            _nextCursor = "";
            _isInitialLoading = true;
            _isLoadingMore = false;
            SetResults([new SearchBoxLoaderItem(), new SearchBoxLoaderItem(), new SearchBoxLoaderItem()]);

            if (ViewModel.SelectedSync is null)
            {
                Logger.LogWarning("SearchItem called but no sync is selected.");
                _isInitialLoading = false;
                ShowNotFound();
                return;
            }

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            try
            {
                var result = await commService.SearchItem(ViewModel.SelectedSync, _currentQuery, "", token);
                if (token.IsCancellationRequested)
                    return;

                _isInitialLoading = false;
                if (result.Cause != ExitCode.Ok)
                {
                    Logger.LogWarning($"SearchItem failed with cause {result.Cause}.");
                    ShowNotFound();
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                if (result.Items.Count == 0)
                {
                    Logger.LogExtended("SearchItem returned empty result.");
                    ShowNotFound();
                    return;
                }

                _nextCursor = result.Cursor;
                List<ISearchBoxResultItem> items = [.. result.Items.Select(r => new SearchBoxResultItem(r))];
                if (!string.IsNullOrEmpty(_nextCursor))
                    items.Add(new SearchBoxLoaderItem(IsLoadMore: true));
                SetResults(items);
            }
            catch (OperationCanceledException)
            {
                // Expected: ignore
            }
        }

        private async void LoadMoreResults()
        {
            if (_isInitialLoading || _isLoadingMore || string.IsNullOrEmpty(_nextCursor) || ViewModel.SelectedSync is null)
                return;

            _isLoadingMore = true;
            var token = _searchCts?.Token ?? CancellationToken.None;
            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();
            try
            {
                var result = await commService.SearchItem(ViewModel.SelectedSync, _currentQuery, _nextCursor, token);
                if (token.IsCancellationRequested)
                    return;

                _isLoadingMore = false;
                if (_results.Count > 0 && _results[^1] is SearchBoxLoaderItem { IsLoadMore: true })
                    _results.RemoveAt(_results.Count - 1);

                if (result.Cause != ExitCode.Ok)
                {
                    Logger.LogWarning($"SearchItem (load more) failed with cause {result.Cause}.");
                    _nextCursor = "";
                    return;
                }

                _nextCursor = result.Cursor;
                foreach (var item in result.Items)
                    _results.Add(new SearchBoxResultItem(item));
                if (!string.IsNullOrEmpty(_nextCursor))
                    _results.Add(new SearchBoxLoaderItem(IsLoadMore: true));
            }
            catch (OperationCanceledException)
            {
                // Expected: ignore
            }
        }

        private async void TitleBarSearchBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
        {
            ISearchBoxResultItem? resultItem = args.ChosenSuggestion as ISearchBoxResultItem;

            // If the user has not selected a suggestion, attempt to find an exact match in the suggestion list.
            // Otherwise, use the first suggestion if available. If no suggestions exist, there is no result to process.
            IEnumerable<ISearchBoxResultItem>? currentSuggestions = sender.ItemsSource as IEnumerable<ISearchBoxResultItem>;
            if (resultItem is null)
                resultItem = currentSuggestions?.FirstOrDefault(i => i.SearchItem?.Name == sender.Text);

            if (resultItem is null)
                resultItem = currentSuggestions?.Any() ?? false ? currentSuggestions.ElementAt(0) : null;

            if (resultItem is null || resultItem.SearchItem is null || !resultItem.IsSelectable)
            {
                Logger.LogInfo("QuerySubmitted but no result match the query.");
                return;
            }

            if (ViewModel.SelectedSync is null)
            {
                Logger.LogWarning("SearchItem SuggestionChosen called but no sync is selected.");
                sender.IsSuggestionListOpen = false;
                return;
            }

            if (!resultItem.SearchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.itemUri(ViewModel.SelectedSync.Drive.DriveId, resultItem.SearchItem.NodeId);
                await Launcher.LaunchUriAsync(url);
                _analyticsService.TrackClick(Analytics.Keys.Category.Search, Analytics.Keys.EventName.OpenItemWeb);
                return;
            }

            // Open the file or folder
            var itemRelativePath = resultItem.SearchItem.Path;
            // Remove leading slash or _privateFolderName or _sharedFolderName folder from the path
            if (itemRelativePath is not null)
            {
                string path = System.IO.Path.Combine(ViewModel.SelectedSync.LocalPath, itemRelativePath);

                if (!Directory.Exists(path))
                {
                    await Utility.OpenFileAsync(path);
                    _analyticsService.TrackClick(Analytics.Keys.Category.Search, Analytics.Keys.EventName.OpenItem);
                    sender.IsSuggestionListOpen = false;
                    return;
                }
                await Utility.OpenFolderSecurely(path);
                _analyticsService.TrackClick(Analytics.Keys.Category.Search, Analytics.Keys.EventName.OpenItem);
            }
        }

        private void SearchResultItem_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if (sender.DataContext is not ISearchBoxResultItem item) return;

            // The trailing loader is realized when the user scrolls near the end of the list.
            if (item is SearchBoxLoaderItem { IsLoadMore: true })
                LoadMoreResults();

            // Walk up the visual tree to find the parent ListViewItem
            var parentItem = sender.FindAscendant<ListViewItem>();
            if (parentItem is not null)
            {
                parentItem.IsHitTestVisible = item.IsSelectable;
                parentItem.IsTabStop = item.IsSelectable;
                parentItem.IsTapEnabled = item.IsSelectable;
            }
        }

        private void Template_PointerEntered(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            FrameworkElement? frameworkElement = sender as FrameworkElement;
            if (frameworkElement is null)
            {
                Logger.LogWarning("sender is expected to be a FrameworkElement.");
                return;
            }

            SearchBoxResultItem? searchResultItem = frameworkElement.DataContext as SearchBoxResultItem;
            if (searchResultItem is null)
                return;

            if (_lastItemOver is SearchBoxResultItem lastItemOver && lastItemOver != searchResultItem)
                lastItemOver.ShowUnavailableState = false;

            if (!searchResultItem.SearchItem?.IsAvailableLocally ?? false)
            {
                searchResultItem.ShowUnavailableState = true;
                _lastItemOver = searchResultItem;
            }
        }

        private void Template_PointerExited(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            if (_lastItemOver is SearchBoxResultItem lastItemOver)
                lastItemOver.ShowUnavailableState = false;
        }
    }

    public class SearchBoxResultItemTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FileTemplate { get; set; }
        public DataTemplate? FolderTemplate { get; set; }
        public DataTemplate? NotFoundTemplate { get; set; }
        public DataTemplate? LoaderTemplate { get; set; }


        protected override DataTemplate? SelectTemplateCore(object item)
        {
            if (item is null)
                return base.SelectTemplateCore(item);

            if (item is not ISearchBoxResultItem resultItem)
                return base.SelectTemplateCore(item);

            if (resultItem.SearchItem is not null)
            {
                return resultItem.SearchItem.Type == NodeType.File ? FileTemplate : FolderTemplate;
            }
            else if (item is SearchBoxNotFoundItem)
            {
                return NotFoundTemplate;
            }
            else if (item is SearchBoxLoaderItem)
            {
                return LoaderTemplate;
            }
            return base.SelectTemplateCore(item);
        }
    }
}
