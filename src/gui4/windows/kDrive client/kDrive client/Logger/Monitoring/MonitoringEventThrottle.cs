using System;
using System.Collections.Generic;

namespace Infomaniak.kDrive.Monitoring
{
    internal sealed class MonitoringEventThrottle
    {
        private const int _limitPerMinute = 3;
        private readonly object _lock = new();
        private readonly Dictionary<(string FilePath, int LineNumber), (DateTime LastSentTime, int Count)> _locations = new();

        public bool TryAcquire(string filePath, int lineNumber, DateTime now)
        {
            lock (_lock)
            {
                var key = (filePath, lineNumber);
                if (!_locations.TryGetValue(key, out var info) || (now - info.LastSentTime).TotalMinutes >= 1)
                {
                    _locations[key] = (now, 1);
                    return true;
                }

                if (info.Count >= _limitPerMinute)
                    return false;

                _locations[key] = (info.LastSentTime, info.Count + 1);
                return true;
            }
        }
    }
}
