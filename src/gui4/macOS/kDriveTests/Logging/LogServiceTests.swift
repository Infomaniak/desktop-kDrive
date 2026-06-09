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

import Foundation
@testable import kDriveCore
import Testing

@Suite("LogService Tests")
struct LogServiceTests {
    @Test("Formatter matches the legacy kDrive log line format")
    func formatterMatchesLegacyFormat() throws {
        let event = LogEvent(
            date: try Self.date(year: 2026, month: 3, day: 24, hour: 14, minute: 8, second: 14, millisecond: 920),
            level: .debug,
            category: "general",
            threadID: "2971261",
            file: "matomoclient.cpp",
            line: 57,
            message: "MatomoClient initialized with URL: https://analytics.infomaniak.com and site ID: 29"
        )

        let formattedLine = LogLineFormatter(timeZone: Self.utcTimeZone).format(event)

        #expect(
            formattedLine == "2026-03-24 14:08:14:920 [D] (2971261) matomoclient.cpp:57 - MatomoClient initialized with URL: https://analytics.infomaniak.com and site ID: 29"
        )
    }

    @Test("Error level uses the Qt critical log marker")
    func errorLevelUsesCriticalMarker() throws {
        #expect(LogLevel.error.logLetter == "C")
    }

    @Test("Service appends formatted lines and reports breadcrumbs for all levels")
    func serviceAppendsLineAndReportsBreadcrumbs() throws {
        let writer = InMemoryLogFileWriter()
        let sentryReporter = SpySentryLogReporter()
        let service = LogService(
            formatter: LogLineFormatter(timeZone: Self.utcTimeZone),
            fileWriter: writer,
            sentryReporter: sentryReporter,
            dateProvider: { try! Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529) },
            threadIDProvider: { "227895" }
        )

        service.log(level: .info, category: "xpc", message: "first line\nsecond line", file: "Folder/File.swift", line: 42)
        service.flush()

        #expect(writer.lines == ["2026-06-09 12:00:46:529 [I] (227895) File.swift:42 - first line\\nsecond line"])
        #expect(sentryReporter.breadcrumbs.map(\.level) == [.info])
        #expect(sentryReporter.capturedEvents.isEmpty)
    }

    @Test("Service captures only error and fatal events")
    func serviceCapturesOnlyErrorAndFatalEvents() throws {
        let sentryReporter = SpySentryLogReporter()
        let service = LogService(
            formatter: LogLineFormatter(timeZone: Self.utcTimeZone),
            fileWriter: InMemoryLogFileWriter(),
            sentryReporter: sentryReporter,
            dateProvider: { try! Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529) },
            threadIDProvider: { "227895" }
        )

        service.log(level: .debug, category: "general", message: "debug", file: "File.swift", line: 1)
        service.log(level: .warning, category: "general", message: "warning", file: "File.swift", line: 2)
        service.log(level: .error, category: "general", message: "error", file: "File.swift", line: 3)
        service.log(level: .fatal, category: "general", message: "fatal", file: "File.swift", line: 4)
        service.flush()

        #expect(sentryReporter.breadcrumbs.map(\.level) == [.debug, .warning, .error, .fatal])
        #expect(sentryReporter.capturedEvents.map(\.level) == [.error, .fatal])
    }

    @Test("File writer uses legacy client session log names")
    func fileWriterUsesLegacyClientSessionNames() throws {
        let fileManager = FileManager.default
        let logDirectory = fileManager.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? fileManager.removeItem(at: logDirectory) }

        let date = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529)
        let writer = try LogFileWriter(logDirectory: logDirectory, date: date, timeZone: Self.utcTimeZone, fileManager: fileManager)
        let service = LogService(
            formatter: LogLineFormatter(timeZone: Self.utcTimeZone),
            fileWriter: writer,
            sentryReporter: SpySentryLogReporter(),
            dateProvider: { date },
            threadIDProvider: { "227895" }
        )

        service.log(level: .debug, category: "xpc", message: "Snd rqst 12 ERROR_INFOLIST_LEGACY(35)", file: "commclient.cpp", line: 122)
        service.flush()

        let contents = try String(contentsOf: writer.fileURL, encoding: .utf8)
        #expect(writer.fileURL.lastPathComponent == "20260609_1200_kDrive_client.log.0")
        #expect(contents == "2026-06-09 12:00:46:529 [D] (227895) commclient.cpp:122 - Snd rqst 12 ERROR_INFOLIST_LEGACY(35)\n")
    }

    private static let utcTimeZone = TimeZone(secondsFromGMT: 0)!

    private static func date(
        year: Int,
        month: Int,
        day: Int,
        hour: Int,
        minute: Int,
        second: Int,
        millisecond: Int
    ) throws -> Date {
        var components = DateComponents()
        components.calendar = Calendar(identifier: .gregorian)
        components.timeZone = utcTimeZone
        components.year = year
        components.month = month
        components.day = day
        components.hour = hour
        components.minute = minute
        components.second = second
        components.nanosecond = millisecond * 1_000_000

        return try #require(components.date)
    }
}

private final class InMemoryLogFileWriter: LogFileWriting {
    private(set) var lines = [String]()

    func append(_ line: String) throws {
        lines.append(line)
    }
}

private final class SpySentryLogReporter: SentryLogReporting {
    private(set) var breadcrumbs = [LogEvent]()
    private(set) var capturedEvents = [LogEvent]()

    func addBreadcrumb(_ event: LogEvent) {
        breadcrumbs.append(event)
    }

    func capture(_ event: LogEvent) {
        capturedEvents.append(event)
    }
}
