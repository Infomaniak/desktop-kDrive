using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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
