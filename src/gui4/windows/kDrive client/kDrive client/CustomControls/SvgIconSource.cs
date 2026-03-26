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
    public class SvgIconSource : ImageIconSource
    {
        private CancellationTokenSource? _refreshCts;

        public SvgIconSource()
        {
            RegisterPropertyChangedCallback(ForegroundProperty, OnDependencyPropertyChanged);
            ImageSource = new SvgImageSource();
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
                typeof(SvgIconSource),
                new PropertyMetadata(null, OnDependencyPropertyChanged));

        public string UriString
        {
            get => (string)GetValue(UriSourceStringProperty);
            set => SetValue(UriSourceStringProperty, value);
        }
        public static readonly DependencyProperty UriSourceStringProperty =
            DependencyProperty.Register(
                nameof(UriString),
                typeof(string),
                typeof(SvgIconSource),
                new PropertyMetadata(null, OnDependencyPropertyChanged));

        private static void OnDependencyPropertyChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is SvgIconSource source)
            {
                source.ScheduleRefresh();
            }
        }

        private void OnDependencyPropertyChanged(DependencyObject sender, DependencyProperty dp)
        {
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

            _refreshCts?.Cancel();
            _refreshCts = new CancellationTokenSource();
            var token = _refreshCts.Token;

            DispatcherQueue.TryEnqueue(async () =>
            {
                try
                {
                    await RefreshSource(token);
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Extended, $"SvgIconSource refresh canceled");
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"SvgIconSource refresh failed: {ex.Message}");
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

                var fullUri = ResolveUri(UriSource);
                if (!File.Exists(fullUri.LocalPath))
                {
                    Logger.Log(Logger.Level.Error, $"SVG file not found: {fullUri.LocalPath}");
                    TryFallback();
                    return;
                }

                token.ThrowIfCancellationRequested();

                var svgDoc = SvgDocument.Open(fullUri.LocalPath);
                ApplyForeground(svgDoc);

                token.ThrowIfCancellationRequested();

                using var memoryStream = new MemoryStream();
                svgDoc.Write(memoryStream);
                memoryStream.Seek(0, SeekOrigin.Begin);

                token.ThrowIfCancellationRequested();

                var svgImageSource = (ImageSource as SvgImageSource);
                if (svgImageSource is null)
                {
                    Logger.Log(Logger.Level.Error, $"ImageSource is not SvgImageSource");
                    TryFallback();
                    return;
                }
                await svgImageSource.SetSourceAsync(memoryStream.AsRandomAccessStream()).AsTask(token);
            }
            catch (OperationCanceledException)
            {
                Logger.Log(Logger.Level.Extended, $"SvgIconSource refresh canceled");
            }
            catch (Exception ex)
            {
                Logger.Log(Logger.Level.Error, $"Failed to render SVG: {UriSource} - {ex.Message}");
                TryFallback();
            }
        }
     
        private void ApplyForeground(SvgDocument svgDoc)
        {
            if (Foreground is SolidColorBrush solid)
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

        private static Uri ResolveUri(Uri source)
        {
            if (source.Scheme != "ms-appx")
                return source;

            var path = source.LocalPath.TrimStart('/');
            var absPath = Path.Combine(AppContext.BaseDirectory, path.Replace('/', Path.DirectorySeparatorChar));
            return new Uri(absPath);
        }

        private void TryFallback()
        {
            try
            {
                ImageSource = new SvgImageSource(UriSource);
            }
            catch
            {
                Logger.Log(Logger.Level.Error, $"Failed to load SVG without recoloring: {UriSource}");
            }
        }
    }
}
