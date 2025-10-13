using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Markup;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using System;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Xml.Linq;

namespace Infomaniak.kDrive.CustomControls
{
    public sealed partial class SvgIcon : PathIcon
    {
        public SvgIcon()
        {
            this.InitializeComponent();
        }

        public Color IconColor
        {
            get => (Color)GetValue(IconColorProperty);
            set
            {
                SetValue(IconColorProperty, value);
            }
        }

        public Uri Uri
        {
            get => (Uri)GetValue(SourceUri);
            set
            {
                SetValue(SourceUri, value);
            }
        }

        public static readonly DependencyProperty IconColorProperty =
            DependencyProperty.Register(
                nameof(IconColor),
                typeof(Color),
                typeof(SvgIcon),
                new PropertyMetadata(Color.Black)
            );

        public static readonly DependencyProperty SourceUri =
            DependencyProperty.Register(
                nameof(Uri),
                typeof(Uri),
                typeof(SvgIcon),
                new PropertyMetadata(null, OnUriChanged)
            );

        private static void OnUriChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SvgIcon icon)
            {
                icon.LoadSvgAsync();
            }
        }

        private async void LoadSvgAsync()
        {
            if (Uri == null)
                return;

            try
            {
                string svgContent = await LoadSvgContentAsync(Uri);

                if (string.IsNullOrEmpty(svgContent))
                {
                    Data = new PathGeometry();
                    return;
                }

                var svgDoc = XDocument.Parse(svgContent);
                XNamespace ns = "http://www.w3.org/2000/svg";

                var pathElement = svgDoc.Descendants(ns + "path");
                string combinedData = string.Empty;
                foreach (var path in pathElement)
                {
                    var fillAttr = path.Attribute("fill");
                    if (fillAttr != null)
                    {
                        fillAttr.Value = $"#{IconColor.R:X2}{IconColor.G:X2}{IconColor.B:X2}";
                    }
                    var dAttr = path.Attribute("d");
                    if (dAttr != null)
                    {
                        combinedData += " " + dAttr.Value;
                    }
                }

                Data = PathMarkupToGeometry(combinedData);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"SVG parsing error: {ex.Message}");
                Data = new PathGeometry(); // fallback
            }
        }

        private async Task<string> LoadSvgContentAsync(Uri uri)
        {
            try
            {
                if (uri.Scheme == "file")
                {
                    return await File.ReadAllTextAsync(uri.LocalPath);
                }
                else if (uri.Scheme == "ms-appx")
                {
                    var path = uri.LocalPath.TrimStart('/');
                    var fullPath = System.IO.Path.Combine(AppContext.BaseDirectory, path.Replace('/', System.IO.Path.DirectorySeparatorChar));
                    return await File.ReadAllTextAsync(fullPath);
                }
                else
                {
                    Logger.Log(Logger.Level.Error, $"Unsupported URI scheme: {uri.Scheme}");
                    return "";
                }
            }
            catch
            {
                Logger.Log(Logger.Level.Error, $"Failed to load SVG from {uri}");
                return "";
            }
        }

        Geometry? PathMarkupToGeometry(string pathMarkup)
        {
            string xaml =
            "<Path " +
            "xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>" +
            "<Path.Data>" + pathMarkup + "</Path.Data></Path>";
            var path = XamlReader.Load(xaml) as Microsoft.UI.Xaml.Shapes.Path;
            Geometry? geometry = path?.Data;
            return geometry;
        }
    }
}
