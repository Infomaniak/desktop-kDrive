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
    public class IsNullToBoolOrVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            var parser = new ParameterParser(parameter);
            bool result = false;
            if (value is null)
            {
                result = !parser.KeyEquals("Inverted", "True");
            }
            else
            {
                result = parser.KeyEquals("Inverted", "True");
            }

            if(targetType == typeof(Microsoft.UI.Xaml.Visibility))
            {
                return result ? Microsoft.UI.Xaml.Visibility.Visible : Microsoft.UI.Xaml.Visibility.Collapsed;
            }
            else if(targetType == typeof(bool))
            {
                return result;
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "IsNullToBoolOrVisibilityConverter: Convert - Unsupported target type.");
                throw new NotSupportedException("IsNullToBoolOrVisibilityConverter: Convert - Unsupported target type.");
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsNullToBoolOrVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}