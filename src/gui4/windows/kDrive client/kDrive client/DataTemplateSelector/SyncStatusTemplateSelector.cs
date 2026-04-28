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

namespace Infomaniak.kDrive.TemplateSelectors
{
    public partial class SyncStatusTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SyncUpToDateTemplate { get; set; }
        public DataTemplate? SyncInProgressTemplate { get; set; }
        public DataTemplate? SyncInPauseTemplate { get; set; }
        public DataTemplate? SyncOfflineTemplate { get; set; }
        public DataTemplate? SyncLoadingTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            // Null value can be passed by IDE designer
            if (item is null)
                return null;

            SyncStatus syncStatus;
            if (item is SyncStatus status)
            {
                syncStatus = status;
            }
            else if (item is ViewModels.Sync sync)
            {
                syncStatus = sync.SyncStatus;
            }
            else
            {
                Logger.Log(Logger.Level.Error, "Unexpected type in SelectTemplateCore");
                return SyncLoadingTemplate;
            }

            switch (syncStatus)
            {
                case SyncStatus.Starting:
                case SyncStatus.PauseAsked:
                case SyncStatus.Undefined:
                    return SyncLoadingTemplate;
                case SyncStatus.Running:
                    return SyncInProgressTemplate;
                case SyncStatus.Paused:
                case SyncStatus.Stopped:
                case SyncStatus.Error:
                    return SyncInPauseTemplate;
                case SyncStatus.Offline:
                    return SyncOfflineTemplate;
                case SyncStatus.Idle:
                    return SyncUpToDateTemplate;
                default:
                    return SyncLoadingTemplate;
            }
        }
    }
}
