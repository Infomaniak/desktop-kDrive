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

using Microsoft.UI.Xaml.Controls;

namespace Infomaniak.kDrive.CustomControls
{
    /// Card identifying the item of a file opening request through its icon, name, and parent folder path, shared
    /// by the progress and the failure pop-up windows. The data context is the <see cref="ViewModels.FileOpeningRequest"/>
    /// of the enclosing window.
    public sealed partial class FileOpeningItemCard : UserControl
    {
        public FileOpeningItemCard()
        {
            InitializeComponent();
        }
    }
}
