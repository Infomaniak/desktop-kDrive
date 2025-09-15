using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace KDrive.Converters
{
    internal class DateTimeToTimeAgoConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
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
                    return  $"{Math.Floor(timeSpan.TotalMinutes)} {resourceLoader.GetString("Global_Minute")}";
                }
                if (timeSpan.TotalHours < 24)
                {
                    return $"{Math.Floor(timeSpan.TotalHours)} {resourceLoader.GetString("Global_Hour")}";
                }
                return $"{Math.Floor(timeSpan.TotalDays)} {resourceLoader.GetString("Global_Day")}";
            }
            Logger.Log(Logger.Level.Warning, "Unexpected value type in DateTimeToTimeAgoConverter");
            return "";
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }
}
