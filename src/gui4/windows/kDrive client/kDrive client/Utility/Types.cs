global using UserId = System.Int64;
global using AccountId = System.Int64;
global using DriveId = System.Int64;
global using SyncId = System.Int64;
global using NodeId = System.String;

global using DbId = System.Int64;
global using ExitCode = System.Int16;
global using ExitCause = System.Int16;
global using SyncPath = System.String;


namespace Infomaniak.kDrive.Types
{

    public enum NodeType
    {
        Unknown,
        File, // File or symlink
        Directory
    }

    public enum SyncStatus
    {
        Undefined,
        Starting,
        Running,
        Idle,
        PauseAsked,
        Paused,
        StopAsked,
        Stopped,
        Error,
        Offline
    };

    public enum SyncFileInstruction
    {
        None,
        Update,
        UpdateMetadata,
        Remove,
        Move,
        Get,
        Put,
        Ignore
    };
    public enum SyncFileStatus
    {
        Unknown,
        Error,
        Success,
        Conflict,
        Inconsistency,
        Ignored,
        Syncing
    };
    public enum ConflictType
    {
        None,
        EditDelete,
        MoveDelete,
        MoveParentDelete,
        CreateParentDelete,
        MoveMoveSource,
        MoveMoveDest,
        MoveCreate,
        CreateCreate,
        EditEdit,
        MoveMoveCycle
    };
    public enum CancelType
    {
        None,
        Create,
        Edit,
        Move,
        Delete,
        AlreadyExistRemote,
        MoveToBinFailed,
        AlreadyExistLocal,
        TmpBlacklisted,
        ExcludedByTemplate,
        Hardlink,
        FileRescued
    };

    public enum InconsistencyType
    {
        None = 0x000,
        Case = 0x001,
        ForbiddenChar = 0x002, // Char unsupported by OS
        ReservedName = 0x004,
        NameLength = 0x008,
        PathLength = 0x010,
        NotYetSupportedChar = 0x020, // Char not yet supported, ie recent Unicode char (ex: U+1FA77 on pre macOS 13.4)
        DuplicateNames = 0x040, // Two items have the same standardized paths with possibly different encodings (Windows 10 and 11).
        ForbiddenCharOnlySpaces = 0x080, // The name contains only spaces (not supported by back end)
        ForbiddenCharEndWithSpace = 0x100, // The name ends with a space
    };

    public enum SyncDirection
    {
        Unknown,
        Up,
        Down
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
    public interface IDrive
    {
        public string Name { get; }
        public System.Drawing.Color Color { get; }
        public DriveId DriveId { get; set; }
        public DbId UserDbId { get; }
        public AccountId AccountId { get; }
        public string AccountName { get; }
    }

    public class DriveAvailable : IDrive
    {
        public DriveId DriveId { get; set; } = 0;
        public UserId UserId { get; set; } = 0;
        public DbId UserDbId { get; set; } = 0;
        public AccountId AccountId { get; set; } = 0;
        public string AccountName { get; set; } = "Account Name"; // TODO: set properly
        public string Name { get; set; } = "";
        public System.Drawing.Color Color { get; set; } = System.Drawing.Color.White;
    }
}

