using System;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;

namespace Infomaniak.kDrive.Monitoring
{
    internal enum MonitoringEventLevel
    {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    }

    internal interface IMonitoringService
    {
        void AddBreadcrumb(string message, MonitoringEventLevel level);

        // <summary>
        // Captures an event with the specified title, message, level, and optional exception.
        // The file path and line number of the calling code are automatically captured using CallerFilePath and CallerLineNumber attributes.
        // Title: The title of the event, it has to be a constant string, it will be used to group events in the monitoring tool.
        //        If empty, the message will be used as the title.
        // Message: The message of the event, it can be a dynamic string, it will be used to describe the event in the monitoring tool.
        // </summary>
        void CaptureEvent(string title, string message, MonitoringEventLevel level, Exception? exception = null,
            [CallerFilePath] string filePath = "?", [CallerLineNumber] int lineNumber = -1);
        void Start([CallerMemberName] string memberName = "?");
        void Stop();
    }
}
