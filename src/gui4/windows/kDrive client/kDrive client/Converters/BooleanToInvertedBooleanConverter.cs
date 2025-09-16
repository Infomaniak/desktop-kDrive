using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class BooleanToInvertedBooleanConverter : IValueConverter
    {
       public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return !boolValue;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToInvertedBooleanConverter: value is not a boolean.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

       public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return !boolValue;
            }
            Logger.Log(Logger.Level.Fatal, "BooleanToInvertedBooleanConverter: value is not a boolean.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
