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
                format = Utility.GetLocalizedString("Converter_DateTimeToTimeAgoConverter_Since");
            }
            else if (parser.KeyEquals("Format", "Ago"))
            {
                format = Utility.GetLocalizedString("Converter_DateTimeToTimeAgoConverter_Ago");
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
                    return Utility.GetLocalizedString("Global_JustNow");
                }
                if (timeSpan.TotalSeconds < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalSeconds)} {Utility.GetLocalizedString("Global_Second")}");
                }
                if (timeSpan.TotalMinutes < 60)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalMinutes)} {Utility.GetLocalizedString("Global_Minute")}");
                }
                if (timeSpan.TotalHours < 24)
                {
                    return String.Format(format, $"{Math.Floor(timeSpan.TotalHours)} {Utility.GetLocalizedString("Global_Hour")}");
                }
                return String.Format(format, $"{Math.Floor(timeSpan.TotalDays)} {Utility.GetLocalizedString("Global_Day")}")  ;
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
