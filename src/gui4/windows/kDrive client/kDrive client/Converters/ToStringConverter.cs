using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class ToStringConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value == null)
                return string.Empty;
            return value.ToString() ?? string.Empty;
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }
}
