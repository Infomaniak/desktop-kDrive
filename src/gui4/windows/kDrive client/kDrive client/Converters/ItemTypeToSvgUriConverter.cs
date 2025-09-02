using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml.Data;
using KDrive.ViewModels;

namespace KDrive.Converters
{
    internal class ItemTypeToSvgUriConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, string language)
        {
            if (value is SyncActivityItemType itemType)
            {
                return itemType switch
                {
                    SyncActivityItemType.Pdf => new Uri("ms-appx:///Assets/Icons/file-pdf.svg"),
                    SyncActivityItemType.Doc => new Uri("ms-appx:///Assets/Icons/file-text.svg"),
                    SyncActivityItemType.Grid => new Uri("ms-appx:///Assets/Icons/file-grid.svg"),
                    SyncActivityItemType.Directory => new Uri("ms-appx:///Assets/Icons/folder.svg"),
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