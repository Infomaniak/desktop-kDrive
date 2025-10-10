using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;
using Infomaniak.kDrive.ViewModels;

namespace Infomaniak.kDrive.Converters
{
    public class IsNullToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var parser = new ParameterParser(parameter);

            if (value is null)
            {
                return (parser.KeyNotEquals("Inverted", "True")) ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
            }
            else
            {
                return (parser.KeyNotEquals("Inverted", "True")) ? Microsoft.UI.Xaml.Visibility.Collapsed : Microsoft.UI.Xaml.Visibility.Visible;
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsNullToVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}