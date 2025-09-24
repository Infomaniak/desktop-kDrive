using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class DoubleToGridLengthConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is double doubleValue)
            {
                return new Microsoft.UI.Xaml.GridLength(doubleValue, Microsoft.UI.Xaml.GridUnitType.Star);
            }
            else if (value is null)
            {
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
            }
            Logger.Log(Logger.Level.Fatal, "DoubleToGridLengthConverter: value is not a double.");
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
