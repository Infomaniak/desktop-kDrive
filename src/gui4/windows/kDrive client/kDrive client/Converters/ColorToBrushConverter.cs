using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml.Media;
using System.Drawing;

namespace Infomaniak.kDrive.Converters
{
    public class ColorToBrushConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is Color color)
            {
                return new SolidColorBrush(Windows.UI.Color.FromArgb(color.A, color.R, color.G, color.B));
            }
            Logger.Log(Logger.Level.Fatal, "ColorToBrushConverter: value is not a Windows.UI.Color.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "ColorToBrushConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}