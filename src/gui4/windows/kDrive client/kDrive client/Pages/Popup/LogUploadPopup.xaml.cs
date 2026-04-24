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
using System;

namespace Infomaniak.kDrive.Pages.Popup
{
    public sealed partial class LogUploadPopup : Page
    {
        private readonly AppModel _viewModel = App.ServiceProvider.GetRequiredService<AppModel>();
        public AppModel ViewModel { get { return _viewModel; } }
        public CheckBox LastSessionCheckBox => CheckBox;
        public LogUploadPopup()
        {
            Logger.Log(Logger.Level.Info, "Navigated to LogUploadPopup - Initializing LogUploadPopup components");
            InitializeComponent();
            Logger.Log(Logger.Level.Debug, "LogUploadPopup components initialized");
        }
    }
}
