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
            if (value is null)
            {
                return Microsoft.UI.Xaml.Visibility.Visible;
            }
            else
            {
                return Microsoft.UI.Xaml.Visibility.Collapsed;
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "IsNullToVisibilityConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}