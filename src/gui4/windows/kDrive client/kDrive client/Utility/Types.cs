global using UserId = System.Int64;
global using AccountId = System.Int64;
global using DriveId = System.Int64;
global using SyncId = System.Int64;
global using NodeId = System.Int64;

global using DbId = System.Int64;
global using ExitCode = System.Int16;
global using ExitCause = System.Int16;
global using SyncPath = System.String;


namespace Infomaniak.kDrive.Types
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
        Idle,
        Pausing,
        Pause,
        Offline
    }

    public enum SyncActivityDirection
    {
        Unknown = 0,
        Outgoing,
        Incoming
    };

    public enum SyncActivityState
    {
        Unknown = 0,
        Successful,
        Failed,
        InProgress
    };

    public enum SyncType
    {
        Unknown,
        Offline,
        Online // (Ex liteSync)
    };

    public enum VersionChannel
    {
        Prod,
        Next,
        Beta,
        Internal,
        Legacy,
        Unknown
    };

    public enum Language
    {
        SystemDefault = 0,
        FR,
        IT,
        DE,
        ES,
        EN
    };
    public enum OAuth2State
    {
        None,
        WaitingForUserAction,
        ProcessingResponse,
        Success,
        Error
    }

    public class DriveAvailable
    {
        public DriveId DriveId { get; set; }
        public UserId UserId { get; set; }
        public DbId UserDbId { get; set; }
        public AccountId AccountId { get; set; }
        public string Name { get; set; }
        public System.Drawing.Color Color { get; set; }
    }
}

