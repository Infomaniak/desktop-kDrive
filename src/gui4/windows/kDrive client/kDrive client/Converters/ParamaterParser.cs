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
using System;
using System.Collections.Generic;
using System.Linq;

namespace Infomaniak.kDrive.Converters
{
    public class ParameterParser
    {
        private readonly Dictionary<string, string>? _parametersDictionary = null;

        public bool IsValid { get => _parametersDictionary is not null; }
        public ParameterParser(object parameter)
        {
            if (parameter is string paramString)
            {
                _parametersDictionary = paramString.Split(';')
                                        .Select(p => p.Split('='))
                                        .Where(p => p.Length == 2)
                                        .ToDictionary(p => p[0].Trim(), p => p[1]);
            }
        }

        public string? Get(string key)
        {
            if (_parametersDictionary is null)
                return null;

            if (_parametersDictionary.TryGetValue(key, out string? value))
            {
                return value;
            }
            else
            {
                return null;
            }
        }

        public bool Contains(string key)
        {
            if (_parametersDictionary is null)
                return false;
            return _parametersDictionary.ContainsKey(key);
        }

        public bool KeyEquals(string key, string value)
        {
            if (_parametersDictionary is null)
                return false;
            if (_parametersDictionary.TryGetValue(key, out string? paramValue))
            {
                return paramValue.Equals(value, StringComparison.OrdinalIgnoreCase);
            }
            else
            {
                return false;
            }
        }

        public bool KeyNotEquals(string key, string value)
        {
            return !KeyEquals(key, value);
        }

    }
}
