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
    public class DateToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            DateTime date;
            if (value is DateTime dateTime)
            {
                date = dateTime;
            }
            else if (value is string)
            {
                if (DateTime.TryParseExact((string)value, "yyyyMMdd", CultureInfo.InvariantCulture, DateTimeStyles.None, out DateTime parsedDate))
                {
                    date = parsedDate;
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "DateToStringConverter: string value is not in the expected format 'yyyyMMdd'.");
                    return string.Empty;
                }
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "DateToStringConverter: value is neither DateTime nor string.");
                throw new InvalidOperationException("Value must be a DateTime or a string in 'yyyyMMdd' format.");
            }

            return date.ToString("D", CultureInfo.CurrentCulture);
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "DateToStringConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}