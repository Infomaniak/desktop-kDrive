using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class BooleanToIntegerConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return boolValue ? 1 : 0;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToIntegerConverter: value is not a boolean.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is int intValue)
            {
                return intValue != 0;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToIntegerConverter: value is not an integer.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }


    }
}
