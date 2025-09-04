global using UserId = System.Int64;
global using DriveId = System.Int64;
global using SyncId = System.Int64;
global using NodeId = System.Int64;

global using DbId = System.Int64;
global using ExitCode = System.Int16;
global using ExitCause = System.Int16;
global using SyncPath = System.String;


namespace KDrive.Types
{

    public enum NodeType
    {
        Directory,
        File,
        Doc,
        Pdf,
        Image,
        Video,
        Audio,
        Grid
    }

    public enum SyncStatus
    {
        Unknown,
        Starting,
        Running,
        Pausing,
        Pause
    }

    public enum SyncDirection
    {
        Unknown = 0,
        Outgoing,
        Incoming,
        EnumEnd
    };
}

