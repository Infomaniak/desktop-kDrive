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

using Infomaniak.kDrive.Monitoring;
using Infomaniak.kDrive.ViewModels;
using Microsoft.Extensions.DependencyInjection;
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
                outputTemplate: "{Timestamp:yyyy-MM-dd HH:mm:ss.fff} [{Level:u1}] ({ThreadId}) {Message}{NewLine}{Exception}")
            .Enrich.WithThreadId().CreateLogger();


        public static Level LogLevel =>
            App.ServiceProvider.GetService<AppModel>()?.Settings.LogLevel ?? Level.Extended;

        public static void LogExtended(string message,
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            Log(Level.Extended, message, "", filePath, lineNumber, memberName);
        }

        public static void LogDebug(string message,
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            Log(Level.Debug, message, "", filePath, lineNumber, memberName);
        }

        public static void LogInfo(string message,
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            Log(Level.Info, message, "", filePath, lineNumber, memberName);
        }

        public static void LogWarning(string message, string title = "",
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            Log(Level.Warning, message, title, filePath, lineNumber, memberName);
        }
        public static void LogError(string message, string title = "",
                [CallerFilePath] string filePath = "?",
                [CallerLineNumber] int lineNumber = -1,
                [CallerMemberName] string memberName = "?")
        {
            Log(Level.Error, message, title, filePath, lineNumber, memberName);
        }

        public static void LogFatal(string message, string title = "",
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            Log(Level.Fatal, message, title, filePath, lineNumber, memberName);
        }

        private static void Log(Level level, string message, string title = "",
            [CallerFilePath] string filePath = "?",
            [CallerLineNumber] int lineNumber = -1,
            [CallerMemberName] string memberName = "?")
        {
            string fileName = Path.GetFileName(filePath);
            string sourceContext = $"{fileName}:{lineNumber} - {memberName}";
            string shortLogEntry = $"{sourceContext}: {message}";

            var monitoringService = App.ServiceProvider.GetRequiredService<IMonitoringService>();
            monitoringService.AddBreadcrumb(shortLogEntry, ToMonitoringEventLevel(level));

            if (level is Level.Warning or Level.Error or Level.Fatal)
                monitoringService.CaptureEvent(title, message, ToMonitoringEventLevel(level), filePath: filePath, lineNumber: lineNumber);

            if (LogLevel == Level.None)
                return;

#if !DEBUG
            if (LogLevel != Level.Extended && LogLevel > level) return;
#endif

            _logger?.Write(ToSerilogLevel(level), "{SourceContext}: {Message}", sourceContext, message);
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


        private static MonitoringEventLevel ToMonitoringEventLevel(Level level) => level switch
        {
            Level.Extended => MonitoringEventLevel.Debug,
            Level.Debug => MonitoringEventLevel.Debug,
            Level.Info => MonitoringEventLevel.Info,
            Level.Warning => MonitoringEventLevel.Warning,
            Level.Error => MonitoringEventLevel.Error,
            Level.Fatal => MonitoringEventLevel.Fatal,
            _ => MonitoringEventLevel.Info
        };
    }
}
