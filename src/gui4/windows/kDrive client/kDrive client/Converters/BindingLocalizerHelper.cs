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

    // This (hack) localizer is used to localize text where x:bind is not possible (e.g. in a DataTemplate using AncestorType binding)
    // It only supports a "key" parameter, which is the key of the string to localize in the resource files.
    // For example: {Binding Converter={StaticResource BindingLocalizerHelper}, ConverterParameter="key=exampleKey"}
    public class BindingLocalizerHelper : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            ParameterParser parser = new(parameter);
            string key = parser.Get("key") ?? string.Empty;

            return Localizer.Instance.GetString(key);
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }
}
