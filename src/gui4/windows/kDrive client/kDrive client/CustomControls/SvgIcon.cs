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
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Svg;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    public partial class SvgIcon : ImageIcon
    {
        private bool _isLoaded;
        private CancellationTokenSource? _refreshCts;
        private long? _foregroundColorToken;
        private List<KeyValuePair<DependencyProperty, long>> _subscriptionTokens = new List<KeyValuePair<DependencyProperty, long>>();

        public SvgIcon()
        {
            // Register property change callbacks
            Foreground = null;
           
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

        public string? ResourceKey
        {
            get => (string?)GetValue(ResourceKeyProperty);
            set => SetValue(ResourceKeyProperty, value);
        }
        public static readonly DependencyProperty ResourceKeyProperty =
            DependencyProperty.Register(
                nameof(ResourceKey),
                typeof(string),
                typeof(SvgIcon),
                new PropertyMetadata(null, OnResourceNameChanged));

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

        private static void OnResourceNameChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
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
            _subscriptionTokens.Add(new KeyValuePair<DependencyProperty, long>(ForegroundProperty, RegisterPropertyChangedCallback(ForegroundProperty, OnForegroundChanged)));
            _subscriptionTokens.Add(new KeyValuePair<DependencyProperty, long>(WidthProperty, RegisterPropertyChangedCallback(WidthProperty, OnDependencyPropertyChanged)));
            _subscriptionTokens.Add(new KeyValuePair<DependencyProperty, long>(HeightProperty, RegisterPropertyChangedCallback(HeightProperty, OnDependencyPropertyChanged)));

            HookForegroundBrush();
            _isLoaded = true;
            ScheduleRefresh();

            // Subscribe to theme and DPI changes
            ActualThemeChanged += OnThemeChanged;
            Utility.DpiHelper.DpiChanged += OnDpiChanged;
        }

        private void OnForegroundChanged(DependencyObject sender, DependencyProperty dp)
        {
            if (dp == ForegroundProperty)
            {
                HookForegroundBrush();
                if (_isLoaded)
                    ScheduleRefresh();
            }
        }

        private void HookForegroundBrush()
        {
            if (_foregroundColorToken.HasValue)
            {
                if (Foreground is SolidColorBrush oldBrush)
                {
                    oldBrush.UnregisterPropertyChangedCallback(
                        SolidColorBrush.ColorProperty,
                        _foregroundColorToken.Value);
                }

                _foregroundColorToken = null;
            }

            if (Foreground is SolidColorBrush brush)
            {
                _foregroundColorToken =
                    brush.RegisterPropertyChangedCallback(
                        SolidColorBrush.ColorProperty,
                        OnForegroundColorChanged);
            }
        }
        private void OnForegroundColorChanged(DependencyObject sender, DependencyProperty dp)
        {
            if (_isLoaded)
                ScheduleRefresh();
        }


        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            _isLoaded = false;
            ActualThemeChanged -= OnThemeChanged;
            Utility.DpiHelper.DpiChanged -= OnDpiChanged;

            // Unregister property change callbacks
            foreach (var token in _subscriptionTokens)
            {
                UnregisterPropertyChangedCallback(token.Key, token.Value);
            }
            if (_foregroundColorToken.HasValue && Foreground is SolidColorBrush brush)
            {
                brush.UnregisterPropertyChangedCallback(
                    SolidColorBrush.ColorProperty,
                    _foregroundColorToken.Value);
            }
            _subscriptionTokens.Clear();
        }

        private void OnThemeChanged(FrameworkElement sender, object args)
        {
            if (_isLoaded)
                ScheduleRefresh();
        }

        private void OnDpiChanged(object? sender, double newScale)
        {
            if (_isLoaded)
                DispatcherQueue?.TryEnqueue(() => ScheduleRefresh());
        }

        private void OnDependencyPropertyChanged(DependencyObject sender, DependencyProperty dp)
        {
            if (_isLoaded)
                ScheduleRefresh();
        }

        private void ScheduleRefresh()
        {
            // If ResourceKey is set, resolve it from application resources
            if (!string.IsNullOrWhiteSpace(ResourceKey))
            {
                if (!Application.Current.Resources.TryGetValue(ResourceKey, out var resource))
                {
                    Logger.Log(Logger.Level.Warning, $"Resource not found: {ResourceKey}");
                    if (UriSource is not null)
                    {
                        UriSource = null;
                        return; // UriSource changed will trigger Refresh
                    }
                }


                string? resourcePath = resource?.ToString();
                if (!string.IsNullOrWhiteSpace(resourcePath))
                {
                    try
                    {
                        var newUri = new Uri(resourcePath);
                        // Only update if the URI actually changed to prevent recursion
                        if (UriSource?.ToString() != newUri.ToString())
                        {
                            UriSource = newUri;
                            return; // UriSource changed will trigger Refresh
                        }
                    }
                    catch
                    {
                        Logger.Log(Logger.Level.Error, $"Failed to create URI from resource: {ResourceKey} -> {resourcePath}");
                        if (UriSource is not null)
                        {
                            UriSource = null;
                            return; // UriSource changed will trigger Refresh
                        }
                    }
                }
                else
                {
                    if (UriSource is not null)
                    {
                        UriSource = null;
                        return; // UriSource changed will trigger Refresh
                    }
                }
            }

            if (!string.IsNullOrEmpty(UriString) && UriString != UriSource?.ToString())
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

            _refreshCts = SvgHelper.ScheduleOnDispatcher(_refreshCts, DispatcherQueue, RefreshSource, "SvgIcon");
        }

        private async Task RefreshSource(CancellationToken token)
        {
            try
            {
                token.ThrowIfCancellationRequested();
                if (UriSource is null)
                    return;

                var foreground = IsIconEnabled ? Foreground : ResolveDisabledForeground();
                var result = SvgHelper.TryLoad(UriSource, foreground, token);
                if (result is null)
                {
                    TryFallback();
                    return;
                }

                using var memoryStream = result.Value.Stream;

                // Compute raster size for current DPI
                var (pixelWidth, pixelHeight) = ComputeRasterSize(result.Value.Doc);

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
        private SolidColorBrush? ResolveDisabledForeground()
        {
            const string key = "TextFillColorDisabledBrush";
            if (Application.Current.Resources.TryGetValue(key, out var res) && res is SolidColorBrush appBrush)
                return appBrush;
            return Foreground as SolidColorBrush;
        }

        private (double Width, double Height) ComputeRasterSize(SvgDocument svgDoc)
        {
            var scale = this.XamlRoot.RasterizationScale;

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
