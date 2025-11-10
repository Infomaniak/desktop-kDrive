using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Infomaniak.kDrive.ViewModels;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;

namespace Infomaniak.kDrive.Converters
{
    public class DriveToHasErrorConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is Drive drive)
            {
                bool hasError = false;
                foreach (var sync in drive.Syncs)
                {
                    if (sync.SyncErrors.Any())
                    {
                        hasError = true;
                        break;
                    }
                }
                switch (targetType)
                {
                    case Type t when t == typeof(bool):
                        return hasError;
                    case Type t when t == typeof(Visibility):
                        return hasError ? Visibility.Visible : Visibility.Collapsed;
                }

            }

            Logger.Log(Logger.Level.Fatal, "DriveToHasErrorConverter: value is not a Drive.");
            throw new ArgumentException("Invalid value type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "DriveToHasErrorConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}
