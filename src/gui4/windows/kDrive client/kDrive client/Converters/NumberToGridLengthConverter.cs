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
    public class NumberToGridLengthConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
            }

            try
            {
                double lenght = System.Convert.ToDouble(value);
                if (lenght < 0)
                    lenght = 0;

                return new Microsoft.UI.Xaml.GridLength(lenght, Microsoft.UI.Xaml.GridUnitType.Star);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error,
                    $"NumberToGridLengthConverter: Unable to convert value '{value}' of type '{value.GetType()}' to double. Exception: {ex}");
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is Microsoft.UI.Xaml.GridLength gridLength)
            {
                return gridLength.Value;
            }
            Logger.Log(Logger.Level.Fatal, "NumberToGridLengthConverter: value is not a GridLength.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
