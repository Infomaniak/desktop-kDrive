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

    public static void Log(Level level, string message, string filePath = "?", int lineNumber = -1, string memberName = "?")
    {
    }
}
