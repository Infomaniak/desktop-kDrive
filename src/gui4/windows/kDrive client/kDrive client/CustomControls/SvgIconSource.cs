/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media.Imaging;
using Svg;
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
            Utility.DpiHelper.DpiChanged += OnDpiChanged;
        }

        private void OnDpiChanged(object? sender, double newScale)
        {
            DispatcherQueue?.TryEnqueue(() => ScheduleRefresh());
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
            _refreshCts = SvgHelper.ScheduleOnDispatcher(_refreshCts, DispatcherQueue, RefreshSource, "SvgIconSource");
        }

        private async Task RefreshSource(CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();
                if (UriSource is null)
                    return;

                var result = SvgHelper.TryLoad(UriSource, Foreground, token);
                if (result is null)
                {
                    TryFallback();
                    return;
                }
                var (pixelWidth, pixelHeight) = ComputeRasterSize(result.Value.Doc);

                ImageSource = new SvgImageSource
                {
                    RasterizePixelWidth = pixelWidth,
                    RasterizePixelHeight = pixelHeight
                };
                var svgImageSource = (SvgImageSource)ImageSource;

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


        private (double Width, double Height) ComputeRasterSize(SvgDocument svgDoc)
        {
            Window? window = (Application.Current as App)?.CurrentWindow;

            if (window is null)
            {
                Logger.Log(Logger.Level.Warning, "Unable to get current window for DPI scaling, defaulting to 1.0");
                return (svgDoc.Width.Value, svgDoc.Height.Value);
            }

            var scale = Utility.DpiHelper.GetScaleForWindow(WinRT.Interop.WindowNative.GetWindowHandle(window));
            scale = scale == 0 ? 1.0 : scale; // Fallback to 1.0 if DPI scaling is unavailable
            return (svgDoc.Height.Value * scale, svgDoc.Width.Value * scale);
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
