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
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml.Media;
using Svg;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.CustomControls
{
    internal static class SvgHelper
    {
        private const int MaxCacheEntries = 128;
        private static readonly ConcurrentDictionary<(string Path, uint Color), byte[]> _svgCache = new();
        private static readonly ConcurrentQueue<(string Path, uint Color)> _cacheOrder = new();
        internal static Uri ResolveUri(Uri source)
        {
            if (source.Scheme != "ms-appx")
                return source;

            var path = source.LocalPath.TrimStart('/');
            var absPath = Path.Combine(AppContext.BaseDirectory, path.Replace('/', Path.DirectorySeparatorChar));
            return new Uri(absPath);
        }

        internal static void ApplyForeground(SvgDocument svgDoc, Brush? foreground)
        {
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

        /// <summary>
        /// Opens the SVG at <paramref name="uriSource"/>, applies <paramref name="foreground"/>,
        /// and serialises the result into a rewound <see cref="MemoryStream"/>.
        /// Returns <c>null</c> when the file does not exist (already logged); throws on other errors.
        /// </summary>
        internal static (SvgDocument Doc, MemoryStream Stream)? TryLoad(
            Uri uriSource, Brush? foreground, CancellationToken token)
        {
            token.ThrowIfCancellationRequested();

            var fullUri = ResolveUri(uriSource);
            if (!File.Exists(fullUri.LocalPath))
            {
                Logger.Log(Logger.Level.Error, $"SVG file not found: {fullUri.LocalPath}");
                return null;
            }

            token.ThrowIfCancellationRequested();

            uint colorKey = 0;
            if (foreground is SolidColorBrush solid)
            {
                var c = solid.Color;
                colorKey = ((uint)c.A << 24) | ((uint)c.R << 16) | ((uint)c.G << 8) | c.B;
            }

            var cacheKey = (fullUri.LocalPath, colorKey);

            if (_svgCache.TryGetValue(cacheKey, out var cachedBytes))
            {
                token.ThrowIfCancellationRequested();
                var cachedMs = new MemoryStream(cachedBytes, writable: false);
                var cachedDoc = SvgDocument.Open<SvgDocument>(cachedMs);
                return (cachedDoc, new MemoryStream(cachedBytes, writable: false));
            }

            var svgDoc = SvgDocument.Open(fullUri.LocalPath);
            ApplyForeground(svgDoc, foreground);

            token.ThrowIfCancellationRequested();

            var ms = new MemoryStream();
            svgDoc.Write(ms);
            var bytes = ms.ToArray();

            if (_svgCache.TryAdd(cacheKey, bytes))
            {
                _cacheOrder.Enqueue(cacheKey);
                while (_svgCache.Count > MaxCacheEntries && _cacheOrder.TryDequeue(out var evictKey))
                {
                    _svgCache.TryRemove(evictKey, out _);
                }
            }

            ms.Seek(0, SeekOrigin.Begin);

            token.ThrowIfCancellationRequested();

            return (svgDoc, ms);
        }

        /// <summary>
        /// Cancels any in-flight refresh, creates a fresh <see cref="CancellationTokenSource"/>,
        /// and enqueues <paramref name="refreshAction"/> on <paramref name="dispatcherQueue"/>.
        /// Returns the new <see cref="CancellationTokenSource"/> so the caller can store it.
        /// </summary>
        internal static CancellationTokenSource ScheduleOnDispatcher(
            CancellationTokenSource? previousCts,
            DispatcherQueue dispatcherQueue,
            Func<CancellationToken, Task> refreshAction,
            string logContext)
        {
            previousCts?.Cancel();
            var newCts = new CancellationTokenSource();
            var token = newCts.Token;

            dispatcherQueue.TryEnqueue(async () =>
            {
                try
                {
                    await refreshAction(token);
                }
                catch (OperationCanceledException)
                {
                    Logger.Log(Logger.Level.Extended, $"{logContext} refresh canceled");
                }
                catch (Exception ex)
                {
                    Logger.Log(Logger.Level.Error, $"{logContext} refresh failed: {ex.Message}");
                }
            });

            return newCts;
        }
    }
}
