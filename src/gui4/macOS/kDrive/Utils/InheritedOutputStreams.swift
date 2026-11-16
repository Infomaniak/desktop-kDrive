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

import Darwin

struct InheritedOutputStreams {
    enum ConfigurationError: Error, Equatable {
        case unableToConfigureDescriptor(Int32, errorCode: Int32)
    }

    func configure(fileDescriptors: [Int32] = [STDOUT_FILENO, STDERR_FILENO]) throws {
        for descriptor in fileDescriptors {
            // The server owns the read ends of these pipes. Logging (including os_log's stderr mirror)
            // must return EPIPE rather than kill the GUI when those readers disappear during shutdown.
            guard fcntl(descriptor, F_SETNOSIGPIPE, 1) != -1 else {
                let errorCode = errno
                // A stream that was already closed cannot generate a broken-pipe signal.
                if errorCode == EBADF { continue }
                throw ConfigurationError.unableToConfigureDescriptor(descriptor, errorCode: errorCode)
            }
        }
    }
}
