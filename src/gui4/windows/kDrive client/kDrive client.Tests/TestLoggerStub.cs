namespace Infomaniak.kDrive;

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
    public static void LogExtended(string message, string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
    public static void LogDebug(string message, string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
    public static void LogInfo(string message, string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
    public static void LogWarning(string message, string title = "", string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
    public static void LogError(string message, string title = "", string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
    public static void LogFatal(string message, string title = "", string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
}
