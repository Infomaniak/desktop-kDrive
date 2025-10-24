using Infomaniak.kDrive.Types;
using Microsoft.UI.Xaml.Data;
using System;
using Microsoft.UI.Xaml;

namespace Infomaniak.kDrive.Converters
{
    public class NodeTypeToIconUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (targetType != typeof(string) && targetType != typeof(Uri))
            {
                Logger.Log(Logger.Level.Fatal, "NodeTypeToSvgUriConverter: Invalid target type.");
                throw new ArgumentException("Invalid target type", nameof(targetType));
            }

            if (value is NodeType nodeType)
            {
                // load icons from app resources
                string result = "";
                try
                {
                    string resourceKey = nodeType switch
                    {
                        NodeType.Pdf => "Infomaniak.DS.Icons.Documents.file-pdf",
                        NodeType.Doc => "Infomaniak.DS.Icons.Documents.file-text",
                        NodeType.Grid => "Infomaniak.DS.Icons.Documents.file-grid",
                        NodeType.Directory => "Infomaniak.DS.Icons.Documents.folder",
                        _ => "Infomaniak.DS.Icons.Documents.file-text",
                    };
                    if (Application.Current.Resources[resourceKey] is string iconUriStr)
                    {
                        result = iconUriStr;
                    }
                    else
                    {
                        Logger.Log(Logger.Level.Error, $"Resource for NodeType {nodeType} should be {resourceKey} but was not found or is not a string.");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"NodeTypeToSvgUriConverter: Failed to get resource for NodeType {nodeType}: {ex.Message}");
                }
                if (targetType == typeof(string))
                    return result;
                else
                    return new Uri(result);
            }
            else
            {
                Logger.Log(Logger.Level.Fatal, "NodeTypeToSvgUriConverter: value is not a NodeType.");
                throw new ArgumentException("Invalid item type", nameof(value));
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            Logger.Log(Logger.Level.Fatal, "NodeTypeToSvgUriConverter: ConvertBack is not implemented.");
            throw new NotImplementedException();
        }
    }
}