/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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
using Infomaniak.kDrive.Types;
using Infomaniak.kDrive.ViewModels;
using System;
using System.Linq;

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
        public bool ShowInSystemTray { get; set; } = false;

        public bool matches(Error error)
        {
            return Levels.Contains(error.ErrorLevel) &&
                   NodeTypes.Contains(error.NodeType) &&
                   CancelTypes.Contains(error.CancelType) &&
                   InconsistencyTypes.Contains(error.InconsistencyType) &&
                   ConflictTypes.Contains(error.ConflictType) &&
                   ExitCodes.Contains(error.ExitCode) &&
                   ExitCauses.Contains(error.ExitCause);
        }
    }
}
