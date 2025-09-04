using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;
using KDrive.ViewModels;

namespace KDrive.Converters
{
    internal class StringPathToFileNameConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is string path)
            {
                return System.IO.Path.GetFileName(path);
            }
            Logger.Log(Logger.Level.Fatal, "StringPathToFileNameConverter: value is not a string.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "StringPathToFileNameConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}