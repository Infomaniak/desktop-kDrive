using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class IsGreaterConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is int intValue)
            {
                int paramInt = 0;
                if (parameter is string paramString && int.TryParse(paramString, out int parsedInt))
                {
                    paramInt = parsedInt;
                }
                else if (parameter is int paramAsInt)
                {
                    paramInt = paramAsInt;
                }
                if (targetType == typeof(bool))
                    return intValue > paramInt;
                else if (targetType == typeof(Microsoft.UI.Xaml.Visibility))
                    return intValue > paramInt ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
                else
                    throw new ArgumentException("Invalid target type", nameof(targetType));
            }
            Logger.Log(Logger.Level.Fatal, "IsGreaterToBooleanConverter: value is not an integer.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsGreaterToBooleanConverter: ConvertBack is not supported.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
