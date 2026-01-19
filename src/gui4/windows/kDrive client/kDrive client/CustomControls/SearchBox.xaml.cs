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

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Infomaniak.kDrive.CustomControls
{
    public interface ISearchBoxResultItem
    {
        SearchItem? SearchItem { get; }
        bool IsSelectable { get; }
    }

    public sealed record SearchBoxResultItem(SearchItem? SearchItem, bool IsSelectable = true) : ISearchBoxResultItem;

    public sealed record SearchBoxLoaderItem() : ISearchBoxResultItem
    {
        public SearchItem? SearchItem => null;
        public bool IsSelectable => false;
    };

    public sealed record SearchBoxNotFoundItem() : ISearchBoxResultItem
    {
        public SearchItem? SearchItem => null;
        public bool IsSelectable => false;
    };

    public sealed partial class SearchBox : UserControl
    {
        private CancellationTokenSource? _searchCts;
        private AppModel _appModel = App.ServiceProvider.GetRequiredService<AppModel>();
        private const string _privateFolderName = "Private/";
        private const string _sharedFolderName = "Shared/";
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

            if (_appModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem called but no sync is selected.");
                sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                return;
            }

            try
            {
                var result = await commService.SearchItem(
                    _appModel.SelectedSync.DbId,
                    sender.Text,
                    token);

                if (token.IsCancellationRequested)
                {
                    return;
                }

                if (result is null)
                {
                    Logger.Log(Logger.Level.Warning, "SearchItem returned null result.");
                    sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                    return;
                }

                List<ISearchBoxResultItem> items = new();

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
            sender.Text = "";

            if (args.ChosenSuggestion is not ISearchBoxResultItem resultItem || resultItem.SearchItem is null || !resultItem.IsSelectable)
            {
                return;
            }

            if (_appModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem SuggestionChosen called but no sync is selected.");
                sender.IsSuggestionListOpen = false;
                return;
            }

            if (!resultItem.SearchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.OpenItemUri(_appModel.SelectedSync.Drive.DriveId, resultItem.SearchItem.NodeId);
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

                string path = System.IO.Path.Combine(_appModel.SelectedSync.LocalPath, itemRelativePath);

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

            if (_appModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem SuggestionChosen called but no sync is selected.");
                return;
            }

            if (!resultItem.SearchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.OpenItemUri(_appModel.SelectedSync.Drive.DriveId, resultItem.SearchItem.NodeId);
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

                string path = System.IO.Path.Combine(_appModel.SelectedSync.LocalPath, itemRelativePath);
                await Utility.OpenFolderSecurely(path);
            }
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
