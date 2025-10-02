using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;
using WinRT;

namespace Infomaniak.kDrive.Converters
{
    public class NumberToGridLengthConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value == null)
            {
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
            }

            try
            {
                return new Microsoft.UI.Xaml.GridLength(System.Convert.ToDouble(value), Microsoft.UI.Xaml.GridUnitType.Star);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Fatal,
                    $"NumberToGridLengthConverter: Unable to convert value '{value}' of type '{value.GetType()}' to double. Exception: {ex}");
                throw;
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is Microsoft.UI.Xaml.GridLength gridLength)
            {
                return gridLength.Value;
            }
            Logger.Log(Logger.Level.Fatal, "NumberToGridLengthConverter: value is not a GridLength.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }
    }
}
