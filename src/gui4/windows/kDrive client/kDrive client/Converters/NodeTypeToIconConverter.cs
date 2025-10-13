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
    public class SyncActivityStateToIconConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncActivityState state)
            {
                return state switch
                {
                    SyncActivityState.InProgress => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "arrows-loop.svg"),
                    SyncActivityState.Failed => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "triangle-alert.svg"),
                    SyncActivityState.Successful => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "circle-check.svg"),
                    _ => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "circle-check.svg")
                };
            }
            Logger.Log(Logger.Level.Fatal, "SyncActivityStateToIconConverter: value is not a SyncActivityState.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "SyncActivityStateToIconConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}