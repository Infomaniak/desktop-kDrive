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