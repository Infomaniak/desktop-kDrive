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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public interface ISearchBoxResultItem
    {
        SearchItem? SearchItem { get; }
        bool IsSelectable { get; }
        bool ShowUnavailableState { get; }
    }

    public sealed class SearchBoxResultItem(SearchItem searchItem) : UISafeObservableObject, ISearchBoxResultItem
    {
        private bool _showUnavailableState = false;

        public SearchItem? SearchItem { get; } = searchItem;
        public bool IsSelectable => true;
        public bool ShowUnavailableState
        {
            get => _showUnavailableState;
            set => SetPropertyInUIThread(ref _showUnavailableState, value);
        }
    }

    public abstract record SearchBoxPlaceholderItem : ISearchBoxResultItem
    {
        public SearchItem? SearchItem => null;
        public bool IsSelectable => false;
        public bool ShowUnavailableState => false;
    }

    // IsLoadMore marks the trailing loader that triggers fetching the next page.
    public sealed record SearchBoxLoaderItem(bool IsLoadMore = false) : SearchBoxPlaceholderItem;

    public sealed record SearchBoxNotFoundItem : SearchBoxPlaceholderItem;

    public class SearchBoxResultItemTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? FileTemplate { get; set; }
        public DataTemplate? FolderTemplate { get; set; }
        public DataTemplate? NotFoundTemplate { get; set; }
        public DataTemplate? LoaderTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item) => item switch
        {
            ISearchBoxResultItem { SearchItem: { } searchItem } => searchItem.Type == NodeType.File ? FileTemplate : FolderTemplate,
            SearchBoxNotFoundItem => NotFoundTemplate,
            SearchBoxLoaderItem => LoaderTemplate,
            _ => base.SelectTemplateCore(item)
        };
    }
}
