using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class ListEmptyToInvertedVisibilityConverter : IValueConverter
    {
       public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is IEnumerable<object> list)
            {
                return !list.Any() ? Visibility.Collapsed : Visibility.Visible;
            }
            Logger.Log(Logger.Level.Fatal, "ListEmptyToInvertedVisibilityConverter: value is not a list.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

       public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "ListEmptyToInvertedVisibilityConverter: ConvertBack is not supported.");
            throw new NotSupportedException("ConvertBack is not supported.");
        }
    }
}
