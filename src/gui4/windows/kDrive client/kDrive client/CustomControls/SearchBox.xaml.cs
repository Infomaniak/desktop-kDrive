using CommunityToolkit.WinUI;
using Infomaniak.kDrive.ServerCommunication.Interfaces;
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

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
            var appModel = App.ServiceProvider.GetRequiredService<AppModel>();

            if (appModel.SelectedSync is null)
            {
                Logger.Log(Logger.Level.Warning, "SearchItem called but no sync is selected.");
                sender.ItemsSource = new List<ISearchBoxResultItem> { new SearchBoxNotFoundItem() };
                return;
            }

            try
            {
                var result = await commService.SearchItem(
                    appModel.SelectedSync.DbId,
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
                return;
            }

            sender.Text = resultItem.SearchItem.Name;
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
                if (item?.SearchItem is not null)
                    parentItem.IsEnabled = item.SearchItem.IsAvailableLocally;
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
