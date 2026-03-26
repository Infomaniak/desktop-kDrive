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

            var units = new (long Threshold, string Label)[]
            {
                (0L,                      Localizer.Instance.GetString("labelBytes")),
                (1024L,                   Localizer.Instance.GetString("labelKiloBytes")),
                (1024L * 1024L,           Localizer.Instance.GetString("labelMegaBytes")),
                (1024L * 1024L * 1024L,   Localizer.Instance.GetString("labelGigaBytes")),
                (1024L * 1024L * 1024L * 1024L, Localizer.Instance.GetString("labelTeraBytes"))
            };

            double displayValue = byteCount;
            string unit = units[0].Label;

            for (int i = units.Length - 1; i >= 0; --i)
            {
                if (byteCount >= units[i].Threshold)
                {
                    displayValue = (double)byteCount / (units[i].Threshold > 0 ? units[i].Threshold : 1);
                    unit = units[i].Label;
                    break;
                }
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
