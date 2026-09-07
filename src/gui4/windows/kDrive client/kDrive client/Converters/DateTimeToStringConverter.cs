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
using System.Globalization;

namespace Infomaniak.kDrive.Converters
{
    public class DateTimeToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            ParameterParser parser = new(parameter);

            string format = parser.Get("format") ?? "d";
            
            if (value is DateTime dateTime)
            {
                return dateTime.ToString(format, CultureInfo.CurrentCulture);
            }

            Logger.Log(Logger.Level.Fatal, $"Unexpected value type is not a {nameof(DateTime)}.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "DateTimeToStringConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
