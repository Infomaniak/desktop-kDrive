using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;

namespace Infomaniak.kDrive.Converters
{
    public class SyncDirectionToToolTipConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncDirection syncDirection)
            {
                return syncDirection switch
                {
                    SyncDirection.Up => Utility.GetLocalizedString("Converter_SyncDirectionToToolTipConverter_Outgoing"),
                    SyncDirection.Down => Utility.GetLocalizedString("Converter_SyncDirectionToToolTipConverter_Incoming"),
                    _ => ""
                };
            }
            Logger.Log(Logger.Level.Fatal, "SyncDirectionToToolTipConverter: value is not a SyncDirection.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncDirectionToToolTipConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}