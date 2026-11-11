/*
 Infomaniak kDrive - Desktop
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import Darwin
import Foundation
@testable import kDrive
import Testing

struct InheritedOutputStreamsTests {
    @Test func closedReaderReturnsBrokenPipeWithoutChangingOtherDescriptors() throws {
        let pipe = Pipe()
        defer { try? pipe.fileHandleForWriting.close() }
        let reader = pipe.fileHandleForReading.fileDescriptor
        let writer = pipe.fileHandleForWriting.fileDescriptor
        #expect(fcntl(reader, F_GETNOSIGPIPE) == 0)
        #expect(fcntl(writer, F_GETNOSIGPIPE) == 0)

        try InheritedOutputStreams().configure(fileDescriptors: [writer])
        #expect(fcntl(writer, F_GETNOSIGPIPE) == 1)
        #expect(fcntl(reader, F_GETNOSIGPIPE) == 0)
        try pipe.fileHandleForReading.close()

        var byte: UInt8 = 0
        let written = write(writer, &byte, 1)
        let errorCode = errno
        #expect(written == -1)
        #expect(errorCode == EPIPE)
    }
}
