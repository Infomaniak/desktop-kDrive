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
    public class IsNullToBoolOrVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var parser = new ParameterParser(parameter);
            bool result = false;
            if (value is null)
                result = !parser.KeyEquals("Inverted", "True");
            else
                result = parser.KeyEquals("Inverted", "True");

            if (targetType == typeof(Microsoft.UI.Xaml.Visibility))
                return result ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
            else if (targetType == typeof(bool))
                return result;

            Logger.Log(Logger.Level.Fatal, "IsNullToBoolOrVisibilityConverter: Convert - Unsupported target type.");
            throw new NotSupportedException("IsNullToBoolOrVisibilityConverter: Convert - Unsupported target type.");
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsNullToBoolOrVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}