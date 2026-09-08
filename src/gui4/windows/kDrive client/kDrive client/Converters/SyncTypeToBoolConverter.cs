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
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class SyncTypeToBoolConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value == null)
                return false;

            if (value is SyncType syncType)
            {
                return syncType == SyncType.Online;
            }

            Logger.Log(Logger.Level.Error, $"Invalid value type for SyncTypeToBoolConverter: expected SyncType, got {value?.GetType().ToString() ?? "null"}");
            return false;

        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return boolValue ? SyncType.Online : SyncType.Offline;
            }
            Logger.Log(Logger.Level.Error, $"Invalid value type for SyncTypeToBoolConverter: expected bool, got {value?.GetType().ToString() ?? "null"}");
            return SyncType.Offline;
        }
    }
}
