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
using System.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SyncIcon : UserControl
    {
        private const string ClassicSyncIconResourceKey = "Infomaniak.DS.Icons.Products.kDrive";
        private const string AdvancedSyncIconUriResourceKey = "Infomaniak.DS.Icons.Documents.folder-outline";

        // DependencyProperty
        public ISync? Sync
        {
            get => (ISync?)GetValue(SyncProperty);
            set => SetValue(SyncProperty, value);
        }

        public string IconUri
        {
            get => (string)GetValue(IconUriProperty);
            set => SetValue(IconUriProperty, value);
        }

        public static readonly DependencyProperty SyncProperty = DependencyProperty.Register(nameof(Sync), typeof(ISync), typeof(SyncIcon), new PropertyMetadata(null, OnSyncChanged));

        private static readonly DependencyProperty IconUriProperty = DependencyProperty.Register(nameof(IconUri), typeof(string), typeof(SyncIcon), new PropertyMetadata(""));

        public SyncIcon()
        {
            this.InitializeComponent();
        }

        private static void OnSyncChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            var control = d as SyncIcon;
            if (control is null)
                return;

            var sync = e.NewValue as ISync;
            string iconKey;
            if (sync is not null && sync.RemoteNodeId is not null && sync.RemoteNodeId.Any())
                iconKey = AdvancedSyncIconUriResourceKey;
            else
                iconKey = ClassicSyncIconResourceKey;

            var application = Application.Current;
            if (application is not null && application.Resources.ContainsKey(iconKey))
                control.IconUri = application.Resources[iconKey] as string ?? "";
            else
                control.IconUri = "";
        }
    }
}
