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
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class IsGreaterConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is int intValue)
            {
                int paramInt = 0;
                if (parameter is string paramString && int.TryParse(paramString, out int parsedInt))
                {
                    paramInt = parsedInt;
                }
                else if (parameter is int paramAsInt)
                {
                    paramInt = paramAsInt;
                }
                if (targetType == typeof(bool))
                    return intValue > paramInt;
                else if (targetType == typeof(Microsoft.UI.Xaml.Visibility))
                    return intValue > paramInt ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
                else
                    throw new ArgumentException("Invalid target type", nameof(targetType));
            }
            Logger.Log(Logger.Level.Fatal, "IsGreaterToBooleanConverter: value is not an integer.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsGreaterToBooleanConverter: ConvertBack is not supported.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
