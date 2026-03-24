using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class DateTimeToTimeAgoConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            ParameterParser parser = new(parameter);

            string format;
            if (!parser.IsValid)
            {
                format = "{0}";
            }
            else if (parser.KeyEquals("Format", "Since"))
            {
                format = Localizer.Instance.GetString("labelSince");
            }
            else if (parser.KeyEquals("Format", "Ago"))
            {
                format = Localizer.Instance.GetString("labelAgo");
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
                    return Localizer.Instance.GetString("labelJustNow");
                }
                if (timeSpan.TotalSeconds < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalSeconds)} {Localizer.Instance.GetString("labelShortSecond")}");
                }
                if (timeSpan.TotalMinutes < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalMinutes)} {Localizer.Instance.GetString("labelShortMinute")}");
                }
                if (timeSpan.TotalHours < 24)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalHours)} {Localizer.Instance.GetString("labelShortHour")}");
                }
                if (timeSpan.TotalDays <= 3) // Only show "x days ago" for up to 3 days, after that show the date
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalDays)} {Localizer.Instance.GetString("labelShortDay")}");
                }
                else
                {
                    return dateTime.ToString("d");
                }
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
