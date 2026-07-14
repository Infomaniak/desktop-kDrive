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

@testable import kDriveCore
import Testing

@Suite("LogLevel KDC Mapping Test")
struct LogLevelKDCMappingTests {
    @Test("Maps every server log level to its file logger counterpart", arguments: [
        (KDC.LogLevel.Debug, LogLevel.debug),
        (KDC.LogLevel.Info, LogLevel.info),
        (KDC.LogLevel.Warning, LogLevel.warning),
        (KDC.LogLevel.Error, LogLevel.error),
        (KDC.LogLevel.Fatal, LogLevel.fatal)
    ])
    func mapsKnownLevels(kdcLevel: KDC.LogLevel, expected: LogLevel) {
        #expect(LogLevel(kdcLogLevel: kdcLevel) == expected)
    }

    @Test("Falls back to info for the sentinel EnumEnd case")
    func mapsSentinelToInfo() {
        #expect(LogLevel(kdcLogLevel: .EnumEnd) == .info)
    }
}
