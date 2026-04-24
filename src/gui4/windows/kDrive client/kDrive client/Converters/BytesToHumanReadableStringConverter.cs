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
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{

    // This converter takes a long (number of bytes) and converts it to a pretty string (e.g. "1.5 GB")
    public class BytesToHumanReadableStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return $"? {Localizer.Instance.GetString("labelMegaBytes")}";
            }

            long byteCount;
            try
            {
                byteCount = System.Convert.ToInt64(value);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Fatal, $"BytesToHumanReadableStringConverter: value is not convertible to a long. Exception: {ex}");
                throw new ArgumentException("Invalid value type", nameof(value));
            }

            if (byteCount < 0)
            {
                return $"? {Localizer.Instance.GetString("labelMegaBytes")}";
            }

            double displayValue;
            string unit;

            const Int64 kiloByte = 1024;
            const Int64 megaByte = kiloByte * 1024;
            const Int64 gigaByte = megaByte * 1024;
            const Int64 teraByte = gigaByte * 1024;

            if (byteCount >= teraByte)
            {
                displayValue = (double)byteCount / teraByte;
                unit = Localizer.Instance.GetString("labelTeraBytes");
            }
            else if (byteCount >= gigaByte)
            {
                displayValue = (double)byteCount / gigaByte;
                unit = Localizer.Instance.GetString("labelGigaBytes");
            }
            else if (byteCount >= megaByte)
            {
                displayValue = (double)byteCount / megaByte;
                unit = Localizer.Instance.GetString("labelMegaBytes");
            }
            else if (byteCount >= kiloByte)
            {
                displayValue = (double)byteCount / kiloByte;
                unit = Localizer.Instance.GetString("labelKiloBytes");
            }
            else
            {
                displayValue = byteCount;
                unit = Localizer.Instance.GetString("labelBytes");
            }

            ParameterParser parameterParser = new ParameterParser(parameter);
            string? decimals = parameterParser.Get("Decimals");
            string template = "0.##"; // default

            if (decimals is not null && int.TryParse(decimals, out int decimalCount) && decimalCount >= 0)
            {
                decimalCount = Math.Clamp(decimalCount, 0, 6);
                template = "0." + new string('#', decimalCount);
                template = template.TrimEnd('.');
            }

            return $"{displayValue.ToString(template)} {unit}";

        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "BytesToHumanReadableStringConverter.ConvertBack should never be called.");
            throw new NotImplementedException("BytesToHumanReadableStringConverter.ConvertBack should never be called.");
        }
    }
}
