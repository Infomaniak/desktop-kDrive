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
using System.IO;
using System.Runtime.CompilerServices;

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
                outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff} [{Level:u3}] {Message}{NewLine}{Exception}")
            .CreateLogger();


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

            SentrySdk.AddBreadcrumb(shortLogEntry, level: ToBreadcrumbLevel(level));

#if !DEBUG
            if (LogLevel > level) return;
#endif

            _logger?.Write(ToSerilogLevel(level), "{SourceContext}: {Message}", sourceContext, message);

#if DEBUG
            if (level > Level.Info)
            {
                System.Diagnostics.Debug.WriteLine($"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [{level}] {shortLogEntry}");
                SentrySdk.CaptureMessage(shortLogEntry, ToSentryLevel(level));
            }
#endif
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
