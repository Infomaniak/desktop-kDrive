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
    public class SyncDirectionToIconConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncDirection syncDirection)
            {
                return syncDirection switch
                {
                    SyncDirection.Outgoing => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "desktop.svg"),
                    SyncDirection.Incoming => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "network.svg"),
                    _ => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "desktop.svg")
                };
            }
            Logger.Log(Logger.Level.Fatal, "SyncDirectionToIconConverter: value is not a SyncDirection.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncDirectionToIconConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}