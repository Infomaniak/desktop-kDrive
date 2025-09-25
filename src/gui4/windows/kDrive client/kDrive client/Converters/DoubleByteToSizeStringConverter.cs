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
    public class DoubleByteToSizeStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var resourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
            if (value is double doubleValue)
            {
                if (doubleValue > 1024.0 * 1024.0 * 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0 * 1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} {resourceLoader.GetString("Global_TeraBytes")}";
                }
                else if (doubleValue > 1024.0 * 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} {resourceLoader.GetString("Global_GigaBytes")}";
                }
                else if (doubleValue > 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} {resourceLoader.GetString("Global_MegaBytes")}";
                }
                else if (doubleValue >= 1024.0)
                {
                    return $"{(doubleValue / 1024.0).ToString("0.##", CultureInfo.InvariantCulture)} {resourceLoader.GetString("Global_KiloBytes")}";
                }
                else
                {
                    return $"{doubleValue.ToString("0.##", CultureInfo.InvariantCulture)} {resourceLoader.GetString("Global_Bytes")}";
                }
            }
            else if (value is null)
            {
                return $"? {resourceLoader.GetString("Global_MegaBytes")}";
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, $"DoubleByteToSizeStringConverter.Convert: value is not a double (value: {value}, type: {value.GetType()})");
                throw new ArgumentException("Value is not a double", nameof(value));
            }
        }
        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "DoubleByteToSizeStringConverter.ConvertBack should never be called.");
            throw new NotImplementedException("DoubleByteToSizeStringConverter.ConvertBack should never be called.");
        }
    }
}
