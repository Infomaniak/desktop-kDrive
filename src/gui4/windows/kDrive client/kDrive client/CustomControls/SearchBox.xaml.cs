using CommunityToolkit.WinUI;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
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

    public sealed record SearchBoxLoaderItem() : ISearchBoxResultItem
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
        private CancellationTokenSource? _searchCts;
        private AppModel ViewModel => App.ServiceProvider.GetRequiredService<AppModel>();
        private const string _privateFolderName = "Private/";
        private const string _sharedFolderName = "Shared/";
        private ISearchBoxResultItem? _lastItemOver;
        public SearchBox()
        {
            InitializeComponent();
        }


        private async void SearchBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {
            if (args.Reason != AutoSuggestionBoxTextChangeReason.UserInput ||
                string.IsNullOrWhiteSpace(sender.Text))
            {
                return;
            }

            // Cancel previous search
            if (_searchCts is not null)
            {
                await _searchCts.CancelAsync();
                _searchCts.Dispose();
            }
            _searchCts = new CancellationTokenSource();
            var token = _searchCts.Token;

            sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxLoaderItem(), new SearchBoxLoaderItem(), new SearchBoxLoaderItem() };

            var commService = App.ServiceProvider.GetRequiredService<IServerCommService>();

            if (ViewModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem called but no sync is selected.");
                sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                return;
            }

            try
            {
                var result = await commService.SearchItem(ViewModel.SelectedSync, sender.Text, token);
                if (token.IsCancellationRequested)
                {
                    return;
                }

                if (result is null)
                {
                    Logger.Log(Logger.Level.Warning, "SearchItem returned null result.");
                    sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                    Utility.ShowUnexpectedErrorTeachingTip();
                    return;
                }

                if (!result.Any())
                {
                    Logger.Log(Logger.Level.Extended, "SearchItem returned empty result.");
                    sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                    return;
                }

                List<ISearchBoxResultItem> items = [];
                if (result.Count == 0)
                {
                    items.Add(new SearchBoxNotFoundItem());
                }
                else
                {
                    items.AddRange(result.Select(r => new SearchBoxResultItem(r)));
                }

                // Move all the unavailable items to the end of the list
                items = items
                    .OrderByDescending(i => i.SearchItem?.IsAvailableLocally ?? false)
                    .ThenBy(i => i.SearchItem?.ModifiedTime)
                    .ThenBy(i => i.SearchItem?.Name)
                    .ToList();

                sender.ItemsSource = items;
            }
            catch (OperationCanceledException)
            {
                // Expected: ignore
            }
        }


        private void TitleBarSearchBox_SuggestionChosen(AutoSuggestBox sender, AutoSuggestBoxSuggestionChosenEventArgs args)
        {
            if (args.SelectedItem is not ISearchBoxResultItem resultItem || resultItem.SearchItem is null || !resultItem.IsSelectable)
            {
                sender.Text = "";
                return;
            }

            sender.Text = resultItem.SearchItem.Name;
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

            sender.Text = "";
            if (resultItem is null || resultItem.SearchItem is null || !resultItem.IsSelectable)
            {
                Logger.Log(Logger.Level.Info, "QuerySubmitted but no result match the query.");
                return;
            }

            if (ViewModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem SuggestionChosen called but no sync is selected.");
                sender.IsSuggestionListOpen = false;
                return;
            }

            if (!resultItem.SearchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.itemUri(ViewModel.SelectedSync.Drive.DriveId, resultItem.SearchItem.NodeId);
                await Launcher.LaunchUriAsync(url);
                sender.IsSuggestionListOpen = false;
                return;
            }

            // Open the file or folder
            var itemRelativePath = resultItem.SearchItem.Path;
            // Remove leading slash or _privateFolderName or _sharedFolderName folder from the path
            if (itemRelativePath is not null)
            {
                itemRelativePath = itemRelativePath.TrimStart('/', '\\');

                if (itemRelativePath.StartsWith(_privateFolderName))
                {
                    itemRelativePath = itemRelativePath[_privateFolderName.Length..];
                }
                else if (itemRelativePath.StartsWith(_sharedFolderName))
                {
                    itemRelativePath = itemRelativePath[_sharedFolderName.Length..];
                }

                string path = System.IO.Path.Combine(ViewModel.SelectedSync.LocalPath, itemRelativePath);

                if (!Directory.Exists(path))
                {
                    await Utility.OpenFileAsync(path);
                    sender.IsSuggestionListOpen = false;
                    return;
                }
                await Utility.OpenFolderSecurely(path);
                sender.IsSuggestionListOpen = false;
            }
        }

        private void SearchResultItem_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if (sender.DataContext is not ISearchBoxResultItem item)
            {
                return;
            }

            // Walk up the visual tree to find the parent ListViewItem
            var parentItem = sender.FindAscendant<ListViewItem>();
            if (parentItem is not null)
            {
                parentItem.IsHitTestVisible = item.IsSelectable;
                parentItem.IsTabStop = item.IsSelectable;
                parentItem.IsTapEnabled = item.IsSelectable;
            }
        }

        private async void Button_PointerPressed(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            var control = sender as Control;
            if (control is null)
            {
                Logger.Log(Logger.Level.Warning, "sender is expected to ba a control.");
                return;
            }

            if (control.DataContext is not ISearchBoxResultItem resultItem || resultItem.SearchItem is null || !resultItem.IsSelectable)
            {
                return;
            }

            if (ViewModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem SuggestionChosen called but no sync is selected.");
                return;
            }

            if (!resultItem.SearchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.itemUri(ViewModel.SelectedSync.Drive.DriveId, resultItem.SearchItem.NodeId);
                await Launcher.LaunchUriAsync(url);
                return;
            }

            var itemRelativePath = resultItem.SearchItem.Path;
            // Remove leading slash or "Private/" or "Shared/" folder from the path
            if (itemRelativePath is not null)
            {
                itemRelativePath = itemRelativePath.TrimStart('/', '\\');

                if (itemRelativePath.StartsWith("Private/"))
                {
                    itemRelativePath = itemRelativePath["Private/".Length..];
                }
                else if (itemRelativePath.StartsWith("Shared/"))
                {
                    itemRelativePath = itemRelativePath["Shared/".Length..];
                }

                string path = System.IO.Path.Combine(ViewModel.SelectedSync.LocalPath, itemRelativePath);
                await Utility.OpenFolderSecurely(path);
            }
        }

        private void Template_PointerEntered(object sender, Microsoft.UI.Xaml.Input.PointerRoutedEventArgs e)
        {
            FrameworkElement? frameworkElement = sender as FrameworkElement;
            if (frameworkElement is null)
            {
                Logger.Log(Logger.Level.Warning, "sender is expected to be a FrameworkElement.");
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
