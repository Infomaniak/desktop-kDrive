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
    internal class ItemTypeToSvgUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is ItemType itemType)
            {
                return itemType switch
                {
                    ItemType.Pdf => new Uri("ms-appx:///Assets/Icons/file-pdf.svg"),
                    ItemType.Doc => new Uri("ms-appx:///Assets/Icons/file-text.svg"),
                    ItemType.Grid => new Uri("ms-appx:///Assets/Icons/file-grid.svg"),
                    ItemType.Directory => new Uri("ms-appx:///Assets/Icons/folder.svg"),
                    // TODO: Add specific icons for audio and video files
                    _ => new Uri("ms-appx:///Assets/Icons/file-text.svg")
                };
            }
            throw new ArgumentException("Invalid item type", nameof(value));
        }

        public object ConvertBack(object value, Type targetType, object parameter, string language)
        {
            throw new NotImplementedException();
        }
    }
}