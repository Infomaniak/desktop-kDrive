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
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

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

        private static string _logFilePath;
        private static StreamWriter? _logStream;

        static Logger()
        {
            string logDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "temp", "kDrive-logdir");
            _logFilePath = Path.Combine(logDir, $"{DateTime.Now:yyyyMMdd_HHmm}_KDriveClient.log");
            int counter = 1;
            while (File.Exists(_logFilePath) && counter < 10)
            {
                _logFilePath = Path.Combine(logDir, $"{DateTime.Now:yyyyMMdd_HHmm}_KDriveClient_{++counter}.log");
            }

            if (counter < 10)
            {
#pragma warning disable S2930
                _logStream = new StreamWriter(_logFilePath, append: true) { AutoFlush = true };
#pragma warning restore S2930
            }
            else
            {
                _logFilePath = "";
                _logStream = null;
            }
        }

        static public string LogFilePath => _logFilePath;
        static public string LogFolder => Path.GetDirectoryName(_logFilePath) ?? "";
        static public Level LogLevel
        {
            get => App.ServiceProvider.GetService<AppModel>()?.Settings.LogLevel ?? Level.Extended;
        }

        public static void Log(Level level, string message, [CallerFilePath] string filePath = "?", [CallerLineNumber] int lineNumber = -1, [CallerMemberName] string memberName = "?")
        {

            string shortLogEntry = $"{Path.GetFileName(filePath)}:{lineNumber} - {memberName}: {message}";
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [{level}] {shortLogEntry}";
            SentrySdk.AddBreadcrumb(shortLogEntry, level: LoggerLevelToBreadcrumbLevel(level));
#if !DEBUG
            if (LogLevel > level || _logStream is null)
                return;
#endif
            try
            {
                _logStream?.WriteLine(logEntry);
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Failed to write log entry: {ex.Message}");
                // Reset the log stream to avoid further errors
                _logStream = new StreamWriter(_logFilePath, append: true) { AutoFlush = true };
            }

#if DEBUG
            if (level > Level.Info)
            {
                System.Diagnostics.Debug.WriteLine(logEntry);
                SentrySdk.CaptureMessage(shortLogEntry, LoggerLevelToSentryLevel(level));
            }
#endif
        }

        private static BreadcrumbLevel LoggerLevelToBreadcrumbLevel(Level level)
        {
            return level switch
            {
                Level.Extended => BreadcrumbLevel.Debug,
                Level.Debug => BreadcrumbLevel.Debug,
                Level.Info => BreadcrumbLevel.Info,
                Level.Warning => BreadcrumbLevel.Warning,
                Level.Error => BreadcrumbLevel.Error,
                Level.Fatal => BreadcrumbLevel.Fatal,
                _ => BreadcrumbLevel.Info,
            };
        }

        private static SentryLevel LoggerLevelToSentryLevel(Level level)
        {
            return level switch
            {
                Level.Extended => SentryLevel.Debug,
                Level.Debug => SentryLevel.Debug,
                Level.Info => SentryLevel.Info,
                Level.Warning => SentryLevel.Warning,
                Level.Error => SentryLevel.Error,
                Level.Fatal => SentryLevel.Fatal,
                _ => SentryLevel.Info,
            };
        }
    }
}
