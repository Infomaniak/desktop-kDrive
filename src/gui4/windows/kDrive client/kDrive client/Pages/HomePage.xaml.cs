/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.VisualBasic.Devices;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Networking.Connectivity;

namespace Infomaniak.kDrive.Pages
{
    public sealed partial class HomePage : Page
    {
        private AppModel _viewModel = ((App)Application.Current).Data;
        public AppModel ViewModel => _viewModel;
        public HomePage()
        {
            Logger.Log(Logger.Level.Info, "Navigated to HomePage - Initializing HomePage components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "HomePage components initialized");
        }


    }
    public class SyncStatusTemplateSelector : DataTemplateSelector
    {
        public DataTemplate? SyncUpToDateTemplate { get; set; }
        public DataTemplate? SyncInProgressTemplate { get; set; }
        public DataTemplate? SyncInPauseTemplate { get; set; }
        public DataTemplate? SyncOfflineTemplate { get; set; }
        public DataTemplate? SyncLoadingTemplate { get; set; }

        protected override DataTemplate? SelectTemplateCore(object item, DependencyObject container)
        {
            // Null value can be passed by IDE designer
            if (item == null)
                return null;
            if (item is SyncStatus syncStatus)
            {
                switch (syncStatus)
                {
                    case SyncStatus.Starting:
                    case SyncStatus.Pausing:
                    case SyncStatus.Unknown:
                        return SyncLoadingTemplate;
                    case SyncStatus.Running:
                        return SyncInProgressTemplate;
                    case SyncStatus.Pause:
                        return SyncInPauseTemplate;
                    case SyncStatus.Offline:
                        return SyncOfflineTemplate;
                    case SyncStatus.Idle:
                        return SyncUpToDateTemplate;
                    default:
                        return SyncLoadingTemplate;
                }
            }
            return SyncLoadingTemplate;
        }
    }
}
