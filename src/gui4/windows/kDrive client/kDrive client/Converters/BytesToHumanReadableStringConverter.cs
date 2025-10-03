using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace Infomaniak.kDrive.Converters
{

    // This converter takes a long (number of bytes) and converts it to a pretty string (e.g. "1.5 GB")
    public class BytesToHumanReadableStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();

            if (value is null)
            {
                return $"? {resourceLoader.GetString("Global_MegaBytes")}";
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

            var units = new (long Threshold, string ResourceKey)[]
            {
                (1L,                      "Global_Bytes"),
                (1024L,                   "Global_KiloBytes"),
                (1024L * 1024L,           "Global_MegaBytes"),
                (1024L * 1024L * 1024L,   "Global_GigaBytes"),
                (1024L * 1024L * 1024L * 1024L, "Global_TeraBytes")
            };

            double displayValue = byteCount;
            string unitKey = units[0].ResourceKey;

            for (int i = units.Length - 1; i >= 0; --i)
            {
                if (byteCount >= units[i].Threshold)
                {
                    displayValue = (double)byteCount / units[i].Threshold;
                    unitKey = units[i].ResourceKey;
                    break;
                }
            }

            return $"{displayValue.ToString("0.##")} {resourceLoader.GetString(unitKey)}";
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "BytesToHumanReadableStringConverter.ConvertBack should never be called.");
            throw new NotImplementedException("BytesToHumanReadableStringConverter.ConvertBack should never be called.");
        }
    }
}
