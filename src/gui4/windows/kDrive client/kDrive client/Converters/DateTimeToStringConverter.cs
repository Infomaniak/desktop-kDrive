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
