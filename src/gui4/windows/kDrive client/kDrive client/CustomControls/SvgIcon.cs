using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Svg;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class SvgIcon : ImageIcon
    {
        private bool _isLoaded;
        private CancellationTokenSource? _refreshCts;

        public SvgIcon()
        {
            // Register property change callbacks
            Foreground = null;
            RegisterPropertyChangedCallback(ForegroundProperty, OnDependencyPropertyChanged);
            RegisterPropertyChangedCallback(WidthProperty, OnDependencyPropertyChanged);
            RegisterPropertyChangedCallback(HeightProperty, OnDependencyPropertyChanged);

            // Track Loaded state
            Loaded += OnLoaded;
            Unloaded += OnUnloaded;
        }

        public Uri? UriSource
        {
            get => (Uri?)GetValue(UriSourceProperty);
            set => SetValue(UriSourceProperty, value);
        }
        public static readonly DependencyProperty UriSourceProperty =
            DependencyProperty.Register(
                nameof(UriSource),
                typeof(Uri),
                typeof(SvgIcon),
                new PropertyMetadata(null, OnUriChanged));

        public string UriString
        {
            get => (string)GetValue(UriSourceStringProperty);
            set => SetValue(UriSourceStringProperty, value);
        }
        public static readonly DependencyProperty UriSourceStringProperty =
            DependencyProperty.Register(
                nameof(UriString),
                typeof(string),
                typeof(SvgIcon),
                new PropertyMetadata(null, OnUriChanged));



        public bool IsIconEnabled
        {
            get => (bool)GetValue(IsIconEnabledProperty);
            set => SetValue(IsIconEnabledProperty, value);
        }
        public static readonly DependencyProperty IsIconEnabledProperty =
            DependencyProperty.Register(
                nameof(IsIconEnabled),
                typeof(bool),
                typeof(SvgIcon),
                new PropertyMetadata(true, OnIconIsEnabledChanged));

        private static void OnUriChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SvgIcon icon && icon._isLoaded)
                icon.ScheduleRefresh();
        }
        private static void OnIconIsEnabledChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SvgIcon icon && icon._isLoaded)
                icon.ScheduleRefresh();
        }

        private void OnLoaded(object sender, RoutedEventArgs e)
        {
            _isLoaded = true;
            ScheduleRefresh();
        }

        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            _isLoaded = false;
        }

        private void OnDependencyPropertyChanged(DependencyObject sender, DependencyProperty dp)
        {
            if (_isLoaded)
                ScheduleRefresh();
        }

        private void ScheduleRefresh()
        {
            if (UriString != UriSource?.ToString())
            {
                try
                {
                    UriSource = new Uri(UriString);
                }
                catch
                {
                    UriSource = null;
                }
                return; // UriSource changed will trigger Refresh
            }
            // Cancel any previous refresh
            _refreshCts?.Cancel();
            _refreshCts = new CancellationTokenSource();
            var token = _refreshCts.Token;

            // Enqueue on UI dispatcher
            DispatcherQueue.TryEnqueue(async () =>
            {
                try
                {
                    await RefreshSource(token);
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Extended, $"SvgIcon refresh canceled");

                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"SvgIcon refresh failed: {ex.Message}");
                }
            });
        }

        private async Task RefreshSource(CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();
                if (UriSource is null)
                    return;

                // Resolve ms-appx URIs
                var fullUri = ResolveUri(UriSource);
                if (!File.Exists(fullUri.LocalPath))
                {
                    Logger.Log(Logger.Level.Error, $"SVG file not found: {fullUri.LocalPath}");
                    TryFallback();
                    return;
                }

                token.ThrowIfCancellationRequested();

                // Load SVG
                var svgDoc = SvgDocument.Open(fullUri.LocalPath);
                ApplyForeground(svgDoc);

                token.ThrowIfCancellationRequested();

                using var memoryStream = new MemoryStream();
                svgDoc.Write(memoryStream);
                memoryStream.Seek(0, SeekOrigin.Begin);

                token.ThrowIfCancellationRequested();

                // Compute raster size for current DPI
                var (pixelWidth, pixelHeight) = ComputeRasterSize(svgDoc);

                var svgImage = new SvgImageSource
                {
                    RasterizePixelWidth = pixelWidth,
                    RasterizePixelHeight = pixelHeight
                };

                await svgImage.SetSourceAsync(memoryStream.AsRandomAccessStream()).AsTask(token);
                token.ThrowIfCancellationRequested();

                Source = svgImage;
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Extended, $"SvgIcon refresh canceled");
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to render SVG: {UriSource} - {ex.Message}");
                TryFallback();
            }
        }
        private static Uri ResolveUri(Uri source)
        {
            if (source.Scheme != "ms-appx")
                return source;

            var path = source.LocalPath.TrimStart('/');
            var absPath = Path.Combine(AppContext.BaseDirectory, path.Replace('/', Path.DirectorySeparatorChar));
            return new Uri(absPath);
        }

        private void ApplyForeground(SvgDocument svgDoc)
        {
            var foreground = IsIconEnabled ? Foreground : ResolveDisabledForeground();
            if (foreground is SolidColorBrush solid)
            {
                var color = System.Drawing.Color.FromArgb(solid.Color.A, solid.Color.R, solid.Color.G, solid.Color.B);
                foreach (var elem in svgDoc.Descendants().OfType<SvgVisualElement>())
                {
                    if (elem.Fill != null && elem.Fill != SvgPaintServer.None)
                        elem.Fill = new SvgColourServer(color);
                    if (elem.Stroke != null && elem.Stroke != SvgPaintServer.None)
                        elem.Stroke = new SvgColourServer(color);
                }
            }
        }

        private SolidColorBrush? ResolveDisabledForeground()
        {
            const string key = "TextFillColorDisabledBrush";
            if (Application.Current.Resources.TryGetValue(key, out var res) && res is SolidColorBrush appBrush)
                return appBrush;
            return Foreground as SolidColorBrush;
        }

        private (double Width, double Height) ComputeRasterSize(SvgDocument svgDoc)
        {
            var scale = Utility.DpiHelper.GetScaleForWindow(
                WinRT.Interop.WindowNative.GetWindowHandle((Application.Current as App)?.CurrentWindow));

            var ratio = svgDoc.Height.Value / svgDoc.Width.Value;
            if (double.IsNaN(ratio) || double.IsInfinity(ratio))
                ratio = 1.0F;


            double targetWidth;

            double targetHeight;
            if (double.IsNaN(Width) && double.IsNaN(Height))
            {
                targetHeight = svgDoc.Height.Value;
                targetWidth = targetHeight / ratio;
            }
            else if (double.IsNaN(Width) && !double.IsNaN(Height))
            {
                targetHeight = Height;
                targetWidth = targetHeight / ratio;
            }
            else if (double.IsNaN(Height) && !double.IsNaN(Width))
            {
                targetWidth = Width;
                targetHeight = targetWidth * ratio;
            }
            else
            {
                targetWidth = Width;
                targetHeight = Height;
            }

            return (targetWidth * scale, targetHeight * scale);
        }

        private void TryFallback()
        {
            try
            {
                Source = new SvgImageSource(UriSource);
            }
            catch
            {
                Logger.Log(Logger.Level.Error, $"Failed to load SVG without recoloring: {UriSource}");
            }
        }
    }
}
