using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class BooleanToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            bool valueAsBool = false;
            // Convert the input value to a boolean
            if (value is bool boolValue)
                valueAsBool = boolValue;
            else if (value is int intValue)
                valueAsBool = intValue != 0;
            else if (value is null)
                valueAsBool = false;
            else
                throw new InvalidOperationException("Value must be a boolean or an integer.");

            // Parse parameters
            ParameterParser parser = new(parameter);
            if (parser.KeyEquals("Inverted", "True"))
                valueAsBool = !valueAsBool;

            return valueAsBool ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "BooleanToVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
