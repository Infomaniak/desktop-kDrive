using Infomaniak.kDrive.ViewModels;
using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Converters
{
    public class SyncDirectionToToolTipConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncDirection syncDirection)
            {
                var ressourceLoader = Windows.ApplicationModel.Resources.ResourceLoader.GetForViewIndependentUse();
                return syncDirection switch
                {
                    SyncDirection.Outgoing => ressourceLoader.GetString("Converter_SyncDirectionToToolTipConverter_Outgoing"),
                    SyncDirection.Incoming => ressourceLoader.GetString("Converter_SyncDirectionToToolTipConverter_Incoming"),
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