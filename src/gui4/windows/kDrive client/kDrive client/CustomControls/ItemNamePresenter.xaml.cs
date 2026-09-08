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
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class ItemNamePresenter : UserControl
    {
        public ItemNamePresenter()
        {
            this.InitializeComponent();
        }

        // DependencyProperty

        // The path or name of the item to display. The control will handle truncation and tooltip if the name is too long.
        public string ItemPath
        {
            get => (string)GetValue(ItemPathProperty);
            set => SetValue(ItemPathProperty, value);
        }

        public NodeType? NodeType
        {
            get => (NodeType)GetValue(NodeTypeProperty);
            set => SetValue(NodeTypeProperty, value);
        }

        public Visibility IsDirectoryIconVisible
        {
            get => NodeType == Types.NodeType.Directory ? Visibility.Visible : Visibility.Collapsed;
        }

        public Visibility IsFileIconVisible
        {
            get => NodeType == Types.NodeType.File ? Visibility.Visible : Visibility.Collapsed;
        }

        public static readonly DependencyProperty ItemPathProperty = DependencyProperty.Register(nameof(ItemPath), typeof(string), typeof(ItemNamePresenter), new PropertyMetadata(null));
        public static readonly DependencyProperty NodeTypeProperty = DependencyProperty.Register(nameof(NodeType), typeof(NodeType), typeof(ItemNamePresenter), new PropertyMetadata(Types.NodeType.File));
    }
}
