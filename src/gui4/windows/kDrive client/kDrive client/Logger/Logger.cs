/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
using Sentry;
using Serilog;
using Serilog.Events;
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Infomaniak.kDrive
{
    public static class Logger
    {
        public enum Level
        {
            Debug,
            Info,
            Warning,
            Error,
            Fatal,
            None,
            Extended
        }

        private static readonly string _logFolder = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "temp",
            "kDrive-logdir");

        public static string LogFolder => _logFolder;

        private static readonly string _logFilePath = Path.Combine(
            _logFolder,
            $"{DateTime.Now:yyyyMMdd_HHmm}_kDriveClient.log");

        private static readonly Serilog.Core.Logger? _logger = new LoggerConfiguration()
            .MinimumLevel.Verbose()
            .WriteTo.File(
                path: _logFilePath,
                fileSizeLimitBytes: 500L * 1024L * 1024L, // 500 MB
                rollOnFileSizeLimit: true,                // roll when exceeding size
                retainedFileCountLimit: 5,                // keep up to 5 rolled files
                outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff} [{Level:u1}] ({ThreadId}) {Message}{NewLine}{Exception}")
            .Enrich.WithThreadId().CreateLogger();


        public static Level LogLevel =>
            App.ServiceProvider.GetService<AppModel>()?.Settings.LogLevel ?? Level.Extended;

        public static void Log(Level level, string message,
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            string fileName = Path.GetFileName(filePath);
            string sourceContext = $"{fileName}:{lineNumber} - {memberName}";
            string shortLogEntry = $"{sourceContext}: {message}";

            if (App.ServiceProvider.GetRequiredService<AppModel>().Settings.SentryEnabled)
                SentrySdk.AddBreadcrumb(shortLogEntry, level: ToBreadcrumbLevel(level));

            if (CanSendSentryEvent(level, filePath, lineNumber))
                SentrySdk.CaptureMessage(shortLogEntry, ToSentryLevel(level));

            if (LogLevel == Level.None)
                return;

#if !DEBUG
            if (LogLevel != Level.Extended && LogLevel > level) return;
#endif

            _logger?.Write(ToSerilogLevel(level), "{SourceContext}: {Message}", sourceContext, message);
        }

        // Manage sentry throttling to avoid flooding the sentry server with too many events (max 3 per minute per unique log location)
        private const int _sentryLogThrottleLimitPerMinute = 3;
        private struct SentryLogThrottleInfo
        {
            public DateTime LastSentTime;
            public int CountInLastMinute;
        }
        private static readonly ConcurrentDictionary<int, SentryLogThrottleInfo> _sentryLogThrottleDict
            = new ConcurrentDictionary<int, SentryLogThrottleInfo>();
        private static bool CanSendSentryEvent(Level level, string filePath, int lineNumber)
        {

            if (!App.ServiceProvider.GetRequiredService<AppModel>().Settings.SentryEnabled)
                return false;

            if (level <= Level.Info || level == Level.Extended)
                return false; // Only send Warning and above

            int hash = HashCode.Combine(filePath, lineNumber);
            var now = DateTime.UtcNow;
            var info = _sentryLogThrottleDict.GetOrAdd(hash, _ => new SentryLogThrottleInfo
            {
                LastSentTime = now,
                CountInLastMinute = 1
            });

            if ((now - info.LastSentTime).TotalMinutes >= 1)
            {
                // Reset count after one minute
                info.CountInLastMinute = 1;
                info.LastSentTime = now;
                _sentryLogThrottleDict[hash] = info;
                return true;
            }
            else if (info.CountInLastMinute < _sentryLogThrottleLimitPerMinute)
            {
                // Allow sending if under the limit
                info.CountInLastMinute++;
                _sentryLogThrottleDict[hash] = info;
                return true;
            }
            else
            {
                SentrySdk.AddBreadcrumb("Sentry log throttled for this location", level: BreadcrumbLevel.Info);
                return false;
            }
        }

        private static IDisposable? _sentryHandler;
        public static void StartSentry([CallerMemberName] string memberName = "?")
        {
            if (_sentryHandler is not null)
                return;

            // Don't call from OnLaunched as stated in Sentry documentation -> https://docs.sentry.io/platforms/dotnet/guides/winui/#install
            if (memberName == "OnLaunched")
            {
                Logger.Log(Logger.Level.Error, "Skipping Sentry initialization in OnLaunched to avoid potential issues with Sentry's WinUI integration.");
#if DEBUG
                throw new InvalidOperationException("Sentry should not be initialized in OnLaunched. Please call Logger.StartSentry() from a earlier/later point in the application lifecycle, such as after the main window has been created.");
#else
                return;
#endif

            }
            // Check if sentry is allowed by the user settings before initializing
            AppModel appModel = App.ServiceProvider.GetRequiredService<AppModel>();
            if (appModel.IsInitialized)
            {
                if (!appModel.Settings.SentryEnabled)
                {
                    Logger.Log(Logger.Level.Info, "Sentry is disabled by user settings, skipping initialization.");
                    return;
                }
            }
            else
            {
                UserDefaults userDefaults = App.ServiceProvider.GetRequiredService<UserDefaults>();
                var sentryEnabledNode = userDefaults.GetValue(nameof(AppModel.Settings.SentryEnabled));
                if (sentryEnabledNode is null)
                {
                    Logger.Log(Logger.Level.Info, "Sentry enabled setting not found in user defaults, skipping initialization.");
                    return;
                }

                if (sentryEnabledNode is JsonValue sentryEnabledValue && sentryEnabledValue.TryGetValue<bool>(out bool sentryEnabled))
                {
                    if (!sentryEnabled)
                    {
                        Logger.Log(Logger.Level.Info, "Sentry is disabled by user defaults, skipping initialization.");
                        return;
                    }
                }
                else
                {
                    Logger.Log(Logger.Level.Warning, "Failed to parse Sentry enabled setting from user defaults, skipping initialization.");
                    return;
                }
            }

            // Initialize Sentry
            StopSentry();
            _sentryHandler = SentrySdk.Init(options =>
            {
                options.Dsn = App.Constants.Sentry.Dsn;
                options.SendDefaultPii = true;
                options.AutoSessionTracking = true;
                options.IsGlobalModeEnabled = true;
                options.Environment = App.Constants.Sentry.Environment;
            });

        }

        public static void StopSentry()
        {
            Sentry.SentrySdk.Close();

            if (_sentryHandler is not null)
            {
                _sentryHandler.Dispose();
                _sentryHandler = null;
            }
        }

        private static LogEventLevel ToSerilogLevel(Level level) => level switch
        {
            Level.Extended => LogEventLevel.Verbose,
            Level.Debug => LogEventLevel.Debug,
            Level.Info => LogEventLevel.Information,
            Level.Warning => LogEventLevel.Warning,
            Level.Error => LogEventLevel.Error,
            Level.Fatal => LogEventLevel.Fatal,
            _ => LogEventLevel.Information
        };

        private static BreadcrumbLevel ToBreadcrumbLevel(Level level) => level switch
        {
            Level.Extended => BreadcrumbLevel.Debug,
            Level.Debug => BreadcrumbLevel.Debug,
            Level.Info => BreadcrumbLevel.Info,
            Level.Warning => BreadcrumbLevel.Warning,
            Level.Error => BreadcrumbLevel.Error,
            Level.Fatal => BreadcrumbLevel.Fatal,
            _ => BreadcrumbLevel.Info
        };

        private static SentryLevel ToSentryLevel(Level level) => level switch
        {
            Level.Extended => SentryLevel.Debug,
            Level.Debug => SentryLevel.Debug,
            Level.Info => SentryLevel.Info,
            Level.Warning => SentryLevel.Warning,
            Level.Error => SentryLevel.Error,
            Level.Fatal => SentryLevel.Fatal,
            _ => SentryLevel.Info
        };
    }
}
