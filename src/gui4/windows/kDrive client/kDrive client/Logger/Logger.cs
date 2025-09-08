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
            Extended,
            Debug,
            Info,
            Warning,
            Error,
            Fatal,
            None
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

        static public Level LogLevel
        {
            get => (App.Current as App)?.Data.Settings.LogLevel ?? Level.Info;
        }

        public static void Log(Level level, string message, [CallerFilePath] string filePath = "?", [CallerLineNumber] int lineNumber = -1, [CallerMemberName] string memberName = "?")
        {
            if (LogLevel > level || _logStream is null) return;
            string logEntry = $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} [{level}] ({Path.GetFileName(filePath)}:{lineNumber}) {memberName}: {message}";
            _logStream.WriteLine(logEntry);
        }
    }
}
