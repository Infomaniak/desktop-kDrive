/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
