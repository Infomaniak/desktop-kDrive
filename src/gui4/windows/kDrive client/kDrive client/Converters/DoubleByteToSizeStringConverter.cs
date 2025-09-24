using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class DoubleByteToSizeStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is double doubleValue)
            {

                if (doubleValue > 1024.0 * 1024.0 * 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0 * 1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} To";
                }
                else if (doubleValue > 1024.0 * 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} Go";
                }
                else if (doubleValue > 1024.0 * 1024.0)
                {
                    return $"{(doubleValue / (1024.0 * 1024.0)).ToString("0.##", CultureInfo.InvariantCulture)} Mo";
                }
                else if (doubleValue >= 1024.0)
                {
                    return $"{(doubleValue / 1024.0).ToString("0.##", CultureInfo.InvariantCulture)} Ko";
                }
                else
                {
                    return $"{doubleValue.ToString("0.##", CultureInfo.InvariantCulture)} octet(s)";
                }
            }
            else if (value is null)
            {
                return "? octet(s)";
            }
            Logger.Log(Logger.Level.Fatal, "DoubleByteToSizeStringConverter: value is not a double.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is Microsoft.UI.Xaml.GridLength gridLength)
            {
                return gridLength.Value;
            }
            Logger.Log(Logger.Level.Fatal, "DoubleToGridLengthConverter: value is not a GridLength.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
