using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class StringIsEmptyConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is string str)
            {
                bool result = string.IsNullOrEmpty(str);
                // Parse parameters
                ParameterParser parser = new(parameter);
                if (parser.KeyEquals("Inverted", "True"))
                    result = !result;

                return result;
            }

            Logger.Log(Logger.Level.Warning, $"StringIsEmptyConverter: Expected a string value, but got {value?.GetType().FullName ?? "null"}. Returning false.");
            return false;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "StringIsEmptyConverter: ConvertBack is not supported.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
