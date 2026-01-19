using Microsoft.UI.Xaml.Data;
using System;
using System.IO;

namespace Infomaniak.kDrive.Converters
{
    public class StringPathToFileNameConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is string path)
            {
                var parser = new ParameterParser(parameter);
                if (parser.KeyEquals("Parent", "True"))
                    path = System.IO.Path.GetDirectoryName(path) ?? "/";

                if (path == "")
                    return path + "/";

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