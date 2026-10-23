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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media.Animation;
using System;
using System.IO;
using System.Linq;
using Windows.System;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SearchBox : UserControl
    {
        private readonly IAnalyticsService _analyticsService = App.ServiceProvider.GetRequiredService<IAnalyticsService>();
        private readonly SearchResultsPager _pager = new(App.ServiceProvider.GetRequiredService<IServerCommService>());
        private AppModel ViewModel => App.ServiceProvider.GetRequiredService<AppModel>();
        private SearchBoxResultItem? _hoveredItem;
        private ListView? _suggestionsList;
        private string _lastOnFocusQuery = "";

        public SearchBox()
        {
            InitializeComponent();
            TitleBarSearchBox.ItemsSource = _pager.Items;
            _pager.LoaderTransitionStarting += (_, _) => SuppressItemTransitionsForNextLayout();
            SetupKeyboardAccelerators();
            GotFocus += OnGotFocus;
            LostFocus += OnLostFocus;
        }

        private void OnLostFocus(object sender, RoutedEventArgs e)
        {
            _lastOnFocusQuery = TitleBarSearchBox.Text;
            TitleBarSearchBox.Text = "";
            _pager.ClearQuery();
        }

        private void OnGotFocus(object sender, RoutedEventArgs e)
        {
            TitleBarSearchBox.Text = _lastOnFocusQuery;
            TitleBarSearchBox.IsSuggestionListOpen = true;
        }

        private void SetupKeyboardAccelerators()
        {
            if (Content is UIElement root)
            {
                var accelerator = new KeyboardAccelerator
                {
                    Key = VirtualKey.F,
                    Modifiers = VirtualKeyModifiers.Control
                };

                accelerator.Invoked += (_, args) =>
                {
                    if (!(TitleBarSearchBox?.Focus(FocusState.Programmatic) ?? false))
                        Logger.Log(Logger.Level.Warning, "Failed to set focus on the search box.");
                    args.Handled = true;
                };

                root.KeyboardAccelerators.Add(accelerator);
            }
        }

        private async void SearchBox_TextChanged(AutoSuggestBox sender, AutoSuggestBoxTextChangedEventArgs args)
        {

            if (string.IsNullOrWhiteSpace(sender.Text))
            {
                _pager.ClearQuery();
                return;
            }

            if (!await _pager.SearchAsync(ViewModel.SelectedSync, sender.Text))
                Utility.ShowUnexpectedErrorTeachingTip();
        }

        // Loader items should appear/disappear instantly, so item transitions are disabled for one layout pass.
        private void SuppressItemTransitionsForNextLayout()
        {
            _suggestionsList ??= TitleBarSearchBox.FindDescendant<Popup>()?.Child is FrameworkElement popupChild
                ? popupChild as ListView ?? popupChild.FindDescendant<ListView>()
                : null;
            if (_suggestionsList is not { } listView || listView.ItemContainerTransitions.Count == 0)
                return;

            var savedTransitions = listView.ItemContainerTransitions;
            listView.ItemContainerTransitions = [];

            void Restore(object? sender, object e)
            {
                listView.LayoutUpdated -= Restore;
                listView.ItemContainerTransitions = savedTransitions;
            }
            listView.LayoutUpdated += Restore;
        }

        private async void TitleBarSearchBox_QuerySubmitted(AutoSuggestBox sender, AutoSuggestBoxQuerySubmittedEventArgs args)
        {
            // Prefer the chosen suggestion, then an exact name match, then the first result.
            var results = _pager.Items.Where(i => i.IsSelectable).ToList();
            var searchItem = (args.ChosenSuggestion as ISearchBoxResultItem)?.SearchItem
                ?? results.FirstOrDefault(i => i.SearchItem?.Name == sender.Text)?.SearchItem
                ?? results.FirstOrDefault()?.SearchItem;

            if (searchItem is null)
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

            if (!searchItem.IsAvailableLocally)
            {
                // Item is not available locally, open in web browser
                var url = App.Constants.Drive.itemUri(ViewModel.SelectedSync.Drive.DriveId, searchItem.NodeId);
                await Launcher.LaunchUriAsync(url);
                _analyticsService.TrackClick(Analytics.Keys.Category.Search, Analytics.Keys.EventName.OpenItemWeb);
                return;
            }

            if (searchItem.Path is null)
                return;

            string path = Path.Combine(ViewModel.SelectedSync.LocalPath, searchItem.Path);
            if (Directory.Exists(path))
            {
                await Utility.OpenFolderSecurely(path);
            }
            else
            {
                await Utility.OpenFileAsync(path);
                sender.IsSuggestionListOpen = false;
            }
            _analyticsService.TrackClick(Analytics.Keys.Category.Search, Analytics.Keys.EventName.OpenItem);
        }

        private void SearchResultItem_DataContextChanged(FrameworkElement sender, DataContextChangedEventArgs args)
        {
            if (sender.DataContext is not ISearchBoxResultItem item) return;

            // The trailing loader is realized when the user scrolls near the end of the list.
            if (item is SearchBoxLoaderItem { IsLoadMore: true })
                _ = _pager.LoadNextPageAsync();

            // Containers are recycled: always reset their state, once loaded if not yet in the tree.
            if (!TryApplyContainerSelectableState(sender))
            {
                void OnLoaded(object s, RoutedEventArgs e)
                {
                    sender.Loaded -= OnLoaded;
                    TryApplyContainerSelectableState(sender);
                }
                sender.Loaded += OnLoaded;
            }
        }

        private static bool TryApplyContainerSelectableState(FrameworkElement element)
        {
            if (element.DataContext is not ISearchBoxResultItem item || element.FindAscendant<ListViewItem>() is not { } container)
                return false;

            container.IsHitTestVisible = item.IsSelectable;
            container.IsTabStop = item.IsSelectable;
            container.IsTapEnabled = item.IsSelectable;
            return true;
        }

        private void Template_PointerEntered(object sender, PointerRoutedEventArgs e)
        {
            if ((sender as FrameworkElement)?.DataContext is not SearchBoxResultItem item || item == _hoveredItem)
                return;

            ClearHoveredItem();
            if (item.SearchItem?.IsAvailableLocally == false)
            {
                item.ShowUnavailableState = true;
                _hoveredItem = item;
            }
        }

        private void Template_PointerExited(object sender, PointerRoutedEventArgs e) => ClearHoveredItem();

        private void ClearHoveredItem()
        {
            if (_hoveredItem is not null)
                _hoveredItem.ShowUnavailableState = false;
            _hoveredItem = null;
        }
    }
}
