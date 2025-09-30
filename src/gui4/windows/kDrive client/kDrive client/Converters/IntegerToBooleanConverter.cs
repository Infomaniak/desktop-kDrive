using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class IntegerToBooleanConverter : IValueConverter
    {
       public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is int intValue)
            {
                return intValue != 0;
            }
            Logger.Log(Logger.Level.Fatal, "IntegerToBooleanConverter: value is not an integer.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

       public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IntegerToBooleanConverter: ConvertBack is not supported.");
            throw new NotSupportedException("ConvertBack is not supported.");
        }
    }
}
