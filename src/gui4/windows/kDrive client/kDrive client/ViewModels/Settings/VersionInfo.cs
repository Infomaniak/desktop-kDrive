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
using System;

namespace Infomaniak.kDrive.ViewModels
{
    public class VersionInfo : UISafeObservableObject
    {
        public VersionChannel Channel { get; set; } = VersionChannel.Unknown;
        public string Tag { get; set; } = string.Empty; // Version number. Example: 3.6.4
        public Int64 buildVersion = 0; // Example: 20240816
    }
}
