using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Media;
using System;
using System.Drawing;

namespace Infomaniak.kDrive.Converters
{
    public class ColorToBrushConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is Color color)
            {   
                return new SolidColorBrush(Microsoft.UI.ColorHelper.FromArgb(color.A, color.R, color.G, color.B));
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