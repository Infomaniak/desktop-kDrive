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
            string prefix = ((parameter as bool?) ?? false) ? "Il y a " : "";
            if (value is DateTime dateTime)
            {
                var timeSpan = DateTime.Now - dateTime;
                var resourceLoader = new Microsoft.Windows.ApplicationModel.Resources.ResourceLoader();

                if (timeSpan.TotalMinutes < 1)
                {
                    return resourceLoader.GetString("Global_JustNow");
                }
                if (timeSpan.TotalMinutes < 60)
                {
                    return $"{prefix} {Math.Floor(timeSpan.TotalMinutes)} {resourceLoader.GetString("Global_Minute")}";
                }
                if (timeSpan.TotalHours < 24)
                {
                    return $"{prefix} {Math.Floor(timeSpan.TotalHours)} {resourceLoader.GetString("Global_Hour")}";
                }
                return $"{prefix} {Math.Floor(timeSpan.TotalDays)} {resourceLoader.GetString("Global_Day")}";
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
