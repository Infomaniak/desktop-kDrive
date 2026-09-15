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

namespace Infomaniak.kDrive.ServerCommunication.Interfaces
{
    /// <summary>
    /// Read-only access to the OS keychain / credential store shared with the kDrive server.
    /// </summary>
    public interface IKeychainStore
    {
        /// <summary>
        /// Reads the raw secret stored under <paramref name="key"/>.
        /// </summary>
        /// <returns>The secret value, or <c>null</c> when no entry exists for the key.</returns>
        string? ReadSecret(string key);
    }
}
