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
    public class NodeTypeToIconConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is NodeType nodeType)
            {
                return nodeType switch
                {
                    NodeType.Pdf => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-pdf.svg"),
                    NodeType.Doc => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-text.svg"),
                    NodeType.Grid => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-grid.svg"),
                    NodeType.Directory => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "folder.svg"),
                    // TODO: Add specific icons for audio and video files
                    _ => AssetLoader.GetAssetUri(AssetLoader.AssetType.Icon, "file-text.svg")
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