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
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Collections;

namespace Infomaniak.kDrive.CustomControls
{

    public sealed partial class SyncSelector : UserControl
    {
        public AppModel ViewModel { get; } =
            App.ServiceProvider.GetRequiredService<AppModel>();

        public SyncSelector()
        {
            InitializeComponent();
        }
    }

    public partial class SyncTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? MainSyncTemplate { get; set; }
        public DataTemplate? AdvancedSyncTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            if (item == null)
                return null;
            if (item is Sync sync)
            {
                if (sync.Drive.MainSync == sync)
                {
                    return MainSyncTemplate;
                }
                else
                {
                    return AdvancedSyncTemplate;
                }
            }
            Logger.Log(Logger.Level.Warning, "SyncTemplateSelector: item is not of type Sync");
            return MainSyncTemplate;
        }
    }
}

