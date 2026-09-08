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
    public class BooleanToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            bool valueAsBool = false;
            // Convert the input value to a boolean
            if (value is bool boolValue)
                valueAsBool = boolValue;
            else if (value is int intValue)
                valueAsBool = intValue != 0;
            else if (value is null)
                valueAsBool = false;
            else
                throw new InvalidOperationException("Value must be a boolean or an integer.");

            // Parse parameters
            ParameterParser parser = new(parameter);
            if (parser.KeyEquals("Inverted", "True"))
                valueAsBool = !valueAsBool;

            return valueAsBool ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "BooleanToVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
