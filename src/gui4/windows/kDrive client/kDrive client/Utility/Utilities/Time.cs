using System;
using System.Globalization;

namespace Infomaniak.kDrive.Utilities
{
    public enum DateTimeToStringMode
    {
        Elapsed,
        ElapsedSinceCapitalized,
        ElapsedSince,
        ElapsedAgo,
        ElapsedAgoCapitalized,
    }

    public class Time
    {
        public static string DateTimeToString(DateTime? value, string format)
        {
            return value?.ToString(format, CultureInfo.CurrentCulture) ?? "?";
        }

        public static string DateTimeElapsedToString(DateTime? value, DateTimeToStringMode mode)
        {
            if (value is null) return "?";

            string format;
            switch (mode)
            {
                case DateTimeToStringMode.ElapsedSince:
                case DateTimeToStringMode.ElapsedSinceCapitalized:
                    format = Localizer.Instance.GetString("labelSince");
                    break;
                case DateTimeToStringMode.ElapsedAgo:
                case DateTimeToStringMode.ElapsedAgoCapitalized:
                    format = Localizer.Instance.GetString("labelAgo");
                    break;
                case DateTimeToStringMode.Elapsed:
                    format = "{0}";
                    break;
                default:
                    Logger.Log(Logger.Level.Warning, "Unknown DateTimeToStringMode.");
                    format = "{0}";
                    break;
            }

            TimeSpan timeSpan = (DateTime.Now - value).GetValueOrDefault();

            string FixCapitalization(string str)
            {
                if (mode == DateTimeToStringMode.ElapsedSinceCapitalized ||
                    mode == DateTimeToStringMode.ElapsedAgoCapitalized)
                {
                    return string.IsNullOrEmpty(str)
                     ? str
                     : char.ToUpper(str[0]) + str[1..];
                }
                else if (mode == DateTimeToStringMode.ElapsedSince ||
                        mode == DateTimeToStringMode.ElapsedAgo)
                {
                    return string.IsNullOrEmpty(str)
                     ? str
                     : char.ToLower(str[0]) + str[1..];
                }
                else
                {
                    return str;
                }
            }

            if (timeSpan.TotalSeconds < 30)
            {
                return FixCapitalization(Localizer.Instance.GetString("labelJustNow"));
            }
            else if (timeSpan.TotalSeconds < 60)
            {
                return FixCapitalization(
                    string.Format(
                        format,
                        $"{Math.Floor(timeSpan.TotalSeconds)} {Localizer.Instance.GetString("labelShortSecond")}"));
            }
            else if (timeSpan.TotalMinutes < 60)
            {
                return FixCapitalization(
                    string.Format(
                        format,
                        $"{Math.Floor(timeSpan.TotalMinutes)} {Localizer.Instance.GetString("labelShortMinute")}"));
            }
            else if (timeSpan.TotalHours < 24)
            {
                return FixCapitalization(
                    string.Format(
                        format,
                        $"{Math.Floor(timeSpan.TotalHours)} {Localizer.Instance.GetString("labelShortHour")}"));
            }
            else if (timeSpan.TotalDays < 4) // Only show "x days ago" for up to 3 days, after that show the date
            {
                return FixCapitalization(
                    string.Format(
                        format,
                        $"{Math.Floor(timeSpan.TotalDays)} {Localizer.Instance.GetString("labelShortDay")}"));
            }
            else
            {
                return value?.ToString("d") ?? "?";
            }
        }

    }
}
