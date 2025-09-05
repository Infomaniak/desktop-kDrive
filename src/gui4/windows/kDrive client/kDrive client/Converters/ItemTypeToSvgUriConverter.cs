using KDrive.ViewModels;
using KDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace KDrive.Converters
{
    public class NodeTypeToSvgUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is NodeType nodeType)
            {
                return nodeType switch
                {
                    NodeType.Pdf => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-pdf"),
                    NodeType.Doc => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-text"),
                    NodeType.Grid => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-grid"),
                    NodeType.Directory => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "folder"),
                    // TODO: Add specific icons for audio and video files
                    _ => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-text")
                };
            }
            Logger.Log(Logger.Level.Fatal, "NodeTypeToSvgUriConverter: value is not a NodeType.");
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "NodeTypeToSvgUriConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}