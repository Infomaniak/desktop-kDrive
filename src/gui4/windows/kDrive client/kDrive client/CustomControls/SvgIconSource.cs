using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using System;
using System.IO;
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

            _refreshCts = SvgHelper.ScheduleOnDispatcher(_refreshCts, DispatcherQueue, RefreshSource, "SvgIconSource");
        }

        private async Task RefreshSource(CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();
                if (UriSource is null)
                    return;

                var svgImageSource = ImageSource as SvgImageSource;
                if (svgImageSource is null)
                {
                    Logger.Log(Logger.Level.Error, $"ImageSource is not SvgImageSource");
                    TryFallback();
                    return;
                }

                var result = SvgHelper.TryLoad(UriSource, Foreground, token);
                if (result is null)
                {
                    TryFallback();
                    return;
                }

                using var memoryStream = result.Value.Stream;
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
