using Infomaniak.kDrive.Types;
using System;

namespace Infomaniak.kDrive.CustomControls.Errors.Templates
{
    [AttributeUsage(AttributeTargets.Class, Inherited = false, AllowMultiple = false)]
    public sealed class ErrorMetadataAttribute : Attribute
    {
        // You can specify multiple values for each property to indicate that the error control
        // can handle multiple types/levels/etc.
        // A property left empty is equivalent to None/Unknown
        public ErrorLevel[] Levels { get; set; } = new[] { ErrorLevel.Unknown };
        public NodeType[] NodeTypes { get; set; } = new[] { NodeType.Unknown };
        public CancelType[] CancelTypes { get; set; } = new[] { CancelType.None };
        public InconsistencyType[] InconsistencyTypes { get; set; } = new[] { InconsistencyType.None };
        public ConflictType[] ConflictTypes { get; set; } = new[] { ConflictType.None };
        public ExitCode[] ExitCodes { get; set; } = new[] { ExitCode.Unknown };
        public ExitCause[] ExitCauses { get; set; } = new[] { ExitCause.Unknown };
    }
}
