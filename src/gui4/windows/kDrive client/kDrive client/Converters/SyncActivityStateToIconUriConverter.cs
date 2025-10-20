using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class SyncActivityStateToIconUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncActivityState state)
            {
                // load icons from app resources
                string result = "";
                try
                {
                    string resourceKey = state switch
                    {
                        SyncActivityState.Successful => "Infomaniak.DS.Icons.Actions.circle-check",
                        SyncActivityState.InProgress => "Infomaniak.DS.Icons.Arrows.arrow-loop",
                        SyncActivityState.Failed => "Infomaniak.DS.Icons.Actions.triangle-alert",
                        _ => "Infomaniak.DS.Icons.Arrows.arrow-loop",
                    };
                    if (Application.Current.Resources[resourceKey] is string iconUriStr)
                    {
                        result = iconUriStr;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, $"Resource for SyncActivityState {state} should be {resourceKey} but was not found or is not a string.");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"Failed to get resource for SyncActivityState {state}: {ex.Message}");
                }
                if (targetType == typeof(string))
                    return result;
                else
                    return new Uri(result);
            }
            Logger.Log(Logger.Level.Fatal, "SyncActivityStateToIconUriConverter: value is not a SyncActivityState.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncActivityStateToIconUriConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}