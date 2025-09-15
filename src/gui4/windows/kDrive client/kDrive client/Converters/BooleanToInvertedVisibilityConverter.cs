using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class BooleanToInvertedVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return boolValue ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
            }
            else if (value is int intValue)
            {
                return intValue != 0 ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToInvertedVisibilityConverter : value is not a boolean.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is Microsoft.UI.Xaml.Visibility visibility)
            {
                return visibility != Microsoft.UI.Xaml.Visibility.Visible;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToInvertedVisibilityConverter : value is not a Visibility.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
