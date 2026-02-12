using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{

    // This (hack) localizer is used to localize text where x:bind is not possible (e.g. in a DataTemplate using AncestorType binding)
    // It only supports a "key" parameter, which is the key of the string to localize in the resource files.
    // For example: {Binding Converter={StaticResource BindingLocalizerHelper}, ConverterParameter="key=exampleKey"}
    public class BindingLocalizerHelper : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            ParameterParser parser = new(parameter);
            string key = parser.Get("key") ?? string.Empty;

            return Localizer.Localizer.Instance.GetString(key);
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }
}
