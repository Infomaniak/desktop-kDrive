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

using kDrive_client.kDrive_client.Log;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace kDrive_client
{
    namespace kDrive_client.Log
    {
        enum Level
        {
            Debug = 0,
            Info = 1,
            Warning = 2,
            Error = 3,
            None = 4
        }
    }
    internal static class Logger
    {
        private static readonly string _logFilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "temp", "kDrive-logdir", "kDrive_client.log");

        //private static readonly string _logFilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "temp", "kDrive-logdir", $"{DateTime.Now:yyyyMMdd_HHmm}_kDrive_client.log");
#pragma warning disable S2930
        private static readonly StreamWriter _logStream = new(_logFilePath, append: true) { AutoFlush = true };
#pragma warning restore S2930 

        static public Level LogLevel { get; set; } = Level.Debug;
        public static void LogInfo(string message, [CallerFilePath] string filePath = "", [CallerLineNumber] int lineNumber = 0)
        {
            if (LogLevel > Level.Info) return;
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [INFO] ({Path.GetFileName(filePath)}:{lineNumber}) {message}";
            _logStream.WriteLine(logEntry);
        }
        public static void LogDebug(string message, [CallerFilePath] string filePath = "", [CallerLineNumber] int lineNumber = 0)
        {
            if (LogLevel > Level.Debug) return;
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [DEBUG] ({Path.GetFileName(filePath)}:{lineNumber}) {message}";
            _logStream.WriteLine(logEntry);
        }

        public static void LogWarning(string message, [CallerFilePath] string filePath = "", [CallerLineNumber] int lineNumber = 0)
        {
            if (LogLevel > Level.Warning) return;
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [WARNING] ({Path.GetFileName(filePath)}:{lineNumber}) {message}";
            _logStream.WriteLine(logEntry);
        }
        public static void LogError(string message, [CallerFilePath] string filePath = "", [CallerLineNumber] int lineNumber = 0)
        {
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [ERROR] ({Path.GetFileName(filePath)}:{lineNumber}) {message}";
            _logStream.WriteLine(logEntry);
        }
    }
}
