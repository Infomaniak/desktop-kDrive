using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class DateTimeToTimeAgoConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            ParameterParser parser = new(parameter);
            var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();

            string format;
            if (!parser.IsValid)
            {
                format = "{0}";
            }
            else if (parser.KeyEquals("Format", "Since"))
            {
                format = resourceLoader.GetString("Converter_DateTimeToTimeAgoConverter_Since");
            }
            else if (parser.KeyEquals("Format", "Ago"))
            {
                format = resourceLoader.GetString("Converter_DateTimeToTimeAgoConverter_Ago");
            }
            else
            {
                Logger.Log(Logger.Level.Warning, "DateTimeToTimeAgoConverter: Unknown parameter format.");
                format = "{0}";
            }

            if (value is DateTime dateTime)
            {
                var timeSpan = DateTime.Now - dateTime;

                if (timeSpan.TotalSeconds < 30)
                {
                    return resourceLoader.GetString("Global_JustNow");
                }
                if (timeSpan.TotalSeconds < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalSeconds)} {resourceLoader.GetString("Global_Second")}");
                }
                if (timeSpan.TotalMinutes < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalMinutes)} {resourceLoader.GetString("Global_Minute")}");
                }
                if (timeSpan.TotalHours < 24)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalHours)} {resourceLoader.GetString("Global_Hour")}");
                }
                return String.Format(format, $"{Math.Floor(timeSpan.TotalDays)} {resourceLoader.GetString("Global_Day")}")  ;
            }
            Logger.Log(Logger.Level.Fatal, $"Unexpected value type is not a {nameof(DateTime)}.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "DateTimeToTimeAgoConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
