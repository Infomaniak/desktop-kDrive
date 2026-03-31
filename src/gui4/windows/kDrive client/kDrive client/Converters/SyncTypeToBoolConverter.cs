using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class SyncTypeToBoolConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value == null)
                return false;

            if (value is SyncType syncType)
            {
                return syncType == SyncType.Online;
            }

            Logger.Log(Logger.Level.Error, $"Invalid value type for SyncTypeToBoolConverter: expected SyncType, got {value.GetType()}");
            return false;

        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            if (value is bool boolValue)
            {
                return boolValue ? SyncType.Online : SyncType.Offline;
            }
            Logger.Log(Logger.Level.Error, $"Invalid value type for SyncTypeToBoolConverter: expected bool, got {value.GetType()}");
            return SyncType.Offline;
        }
    }
}
