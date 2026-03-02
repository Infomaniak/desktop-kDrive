using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Media;
using System;

namespace Infomaniak.kDrive.Converters
{
    /// <summary>
    /// Converts a boolean selection state to the appropriate border brush for conflict version presenters.
    /// Returns AccentFillColorDefaultBrush when selected, CardStrokeColorDefaultBrush when not selected.
    /// </summary>
    public class ConflictSelectionBorderBrushConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            bool isSelected = false;

            // Convert the input value to a boolean
            if (value is bool boolValue)
                isSelected = boolValue;
            else
                throw new InvalidOperationException("Value must be a boolean.");

            // Return AccentFillColorDefaultBrush if selected, CardStrokeColorDefaultBrush if not
            if (isSelected)
            {
                return Application.Current.Resources["AccentFillColorDefaultBrush"] as SolidColorBrush;
            }
            else
            {
                return Application.Current.Resources["CardStrokeColorDefaultBrush"] as SolidColorBrush;
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "ConflictSelectionBorderBrushConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
