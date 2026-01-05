using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;
using Microsoft.UI.Xaml;

namespace Infomaniak.kDrive.Converters
{
    public class SyncActivityDirectionToIconUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncDirection direction)
            {
                // load icons from app resources
                string result = "";
                try
                {
                    string resourceKey = direction switch
                    {
                        SyncDirection.Up => "Infomaniak.DS.Icons.Devices.computer",
                        SyncDirection.Down => "Infomaniak.DS.Icons.Network.globe",
                        _ => "Infomaniak.DS.Icons.Devices.computer",
                    };
                    if (Application.Current.Resources[resourceKey] is string iconUriStr)
                    {
                        result = iconUriStr;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, $"Resource for SyncActivityDirection {direction} should be {resourceKey} but was not found or is not a string.");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to get resource for SyncActivityDirection {direction}: {ex.Message}");
                }
                if (targetType == typeof(string))
                    return result;
                else
                    return new Uri(result);
            }
            Logger.Log(Logger.Level.Fatal, "SyncActivityDirectionToIconUriConverter: value is not a SyncActivityDirection.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncActivityDirectionToIconUriConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}