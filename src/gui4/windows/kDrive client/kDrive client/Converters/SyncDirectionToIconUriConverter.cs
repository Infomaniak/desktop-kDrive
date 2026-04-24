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
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class SyncActivityDirectionToIconUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncDirection direction)
            {
                // load icons from app resources
                string result = "";
                try
                {
                    string resourceKey = direction switch
                    {
                        SyncDirection.Up => "Infomaniak.Custom.Icons.Devices.computer-sync",
                        SyncDirection.Down => "Infomaniak.Custom.Icons.Devices.cloud-sync",
                        _ => "Infomaniak.Custom.Icons.Devices.computer-sync",
                    };
                    if (Application.Current.Resources[resourceKey] is string iconUriStr)
                    {
                        result = iconUriStr;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, $"Resource for SyncActivityDirection {direction} should be {resourceKey} but was not found or is not a string.");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to get resource for SyncActivityDirection {direction}: {ex.Message}");
                }
                if (targetType == typeof(string))
                    return result;
                else
                    return new Uri(result);
            }
            Logger.Log(Logger.Level.Fatal, "SyncActivityDirectionToIconUriConverter: value is not a SyncActivityDirection.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncActivityDirectionToIconUriConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}