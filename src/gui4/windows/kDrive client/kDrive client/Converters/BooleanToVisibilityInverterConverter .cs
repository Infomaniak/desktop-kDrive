using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace KDriveClient.Converters
{
    internal class BooleanToVisibilityInverterConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return boolValue ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
            }
            Logger.Log(Logger.Level.Warning, "BooleanToVisibilityInverterConverter: value is not a boolean.");
            return Microsoft.UI.Xaml.Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is Microsoft.UI.Xaml.Visibility visibility)
            {
                return visibility != Microsoft.UI.Xaml.Visibility.Visible;
            }
            Logger.Log(Logger.Level.Warning, "BooleanToVisibilityInverterConverter: value is not a Visibility.");
            return false;
        }
    }
}
