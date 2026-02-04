using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class NumberToGridLengthConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is null)
            {
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
            }

            try
            {
                double lenght = System.Convert.ToDouble(value);
                if (lenght < 0)
                    lenght = 0;

                return new Microsoft.UI.Xaml.GridLength(lenght, Microsoft.UI.Xaml.GridUnitType.Star);
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Fatal,
                    $"NumberToGridLengthConverter: Unable to convert value '{value}' of type '{value.GetType()}' to double. Exception: {ex}");
                return new Microsoft.UI.Xaml.GridLength(0, Microsoft.UI.Xaml.GridUnitType.Star);
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
