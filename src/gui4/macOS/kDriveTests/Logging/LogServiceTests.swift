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
        let event = try LogEvent(
            date: Self.date(year: 2026, month: 3, day: 24, hour: 14, minute: 8, second: 14, millisecond: 920),
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
    func errorLevelUsesCriticalMarker() {
        #expect(LogLevel.error.logLetter == "C")
    }

    @Test("Service appends formatted lines and reports breadcrumbs for all levels")
    func serviceAppendsLineAndReportsBreadcrumbs() {
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

    @Test("Service only writes events at or above the minimum file level to the file")
    func serviceFiltersFileWritesByMinimumLevel() throws {
        let writer = InMemoryLogFileWriter()
        let sentryReporter = SpySentryLogReporter()
        let date = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529)
        let service = LogService(
            formatter: LogLineFormatter(timeZone: Self.utcTimeZone),
            fileWriter: writer,
            sentryReporter: sentryReporter,
            dateProvider: { date },
            threadIDProvider: { "227895" },
            minimumFileLevel: .warning
        )

        service.log(level: .debug, category: "general", message: "debug", file: "File.swift", line: 1)
        service.log(level: .info, category: "general", message: "info", file: "File.swift", line: 2)
        service.log(level: .warning, category: "general", message: "warning", file: "File.swift", line: 3)
        service.log(level: .error, category: "general", message: "error", file: "File.swift", line: 4)
        service.flush()

        // Only warning and above reach the file...
        #expect(writer.lines == [
            "2026-06-09 12:00:46:529 [W] (227895) File.swift:3 - warning",
            "2026-06-09 12:00:46:529 [C] (227895) File.swift:4 - error"
        ])
        // ...while Sentry breadcrumbs still fire for every level.
        #expect(sentryReporter.breadcrumbs.map(\.level) == [.debug, .info, .warning, .error])
    }

    @Test("Service captures only error and fatal events")
    func serviceCapturesOnlyErrorAndFatalEvents() {
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

    @Test("File writer appends to a date-stamped rolling client log file")
    func fileWriterAppendsToDateStampedRollingClientLog() throws {
        let fileManager = FileManager.default
        let logDirectory = fileManager.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? fileManager.removeItem(at: logDirectory) }

        let date = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529)
        let writer = try LogFileWriter(
            logDirectory: logDirectory,
            date: date,
            timeZone: Self.utcTimeZone,
            fileManager: fileManager
        )
        let service = LogService(
            formatter: LogLineFormatter(timeZone: Self.utcTimeZone),
            fileWriter: writer,
            sentryReporter: SpySentryLogReporter(),
            dateProvider: { date },
            threadIDProvider: { "227895" }
        )

        service.log(
            level: .debug,
            category: "xpc",
            message: "Snd rqst 12 ERROR_INFOLIST_LEGACY(35)",
            file: "commclient.cpp",
            line: 122
        )
        service.flush()

        let contents = try String(contentsOf: writer.fileURL, encoding: .utf8)
        #expect(writer.fileURL.lastPathComponent == "20260609_1200_kDrive_client.log")
        #expect(contents == "2026-06-09 12:00:46:529 [D] (227895) commclient.cpp:122 - Snd rqst 12 ERROR_INFOLIST_LEGACY(35)\n")
    }

    @Test("File writer rotates oversized logs into compressed backups and trims old ones")
    func fileWriterRotatesIntoCompressedBackups() throws {
        let fileManager = FileManager.default
        let logDirectory = fileManager.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? fileManager.removeItem(at: logDirectory) }

        // Tiny size + a small backup count so a handful of lines trigger several rotations.
        // Every single line is larger than `maxFileSize`, so each append after the first rotates.
        let date = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 46, millisecond: 529)
        let maxBackupIndex = 2
        let writer = try LogFileWriter(
            logDirectory: logDirectory,
            date: date,
            timeZone: Self.utcTimeZone,
            fileManager: fileManager,
            maxFileSize: 10,
            maxBackupIndex: maxBackupIndex
        )

        let lineCount = 12
        for index in 0 ..< lineCount {
            try writer.append("line \(index) - padding padding padding padding padding")
        }

        // Backup names share the active file's date-stamped base name.
        let baseName = writer.fileURL.lastPathComponent
        #expect(baseName == "20260609_1200_kDrive_client.log")
        func backupURL(_ index: Int) -> URL {
            logDirectory.appendingPathComponent("\(baseName).\(index).gz", isDirectory: false)
        }

        // The active file exists and only holds the most recent line.
        let activeContents = try String(contentsOf: writer.fileURL, encoding: .utf8)
        #expect(activeContents == "line \(lineCount - 1) - padding padding padding padding padding\n")

        // Backups `.0.gz` … `.maxBackupIndex.gz` exist and are real gzip files (magic 0x1f 0x8b).
        for index in 0 ... maxBackupIndex {
            let url = backupURL(index)
            #expect(fileManager.fileExists(atPath: url.path))

            let header = try Data(contentsOf: url).prefix(2)
            #expect(Array(header) == [0x1F, 0x8B])
        }

        // Nothing is kept beyond the maximum backup index (oldest removed).
        #expect(!fileManager.fileExists(atPath: backupURL(maxBackupIndex + 1).path))
        #expect(!fileManager.fileExists(atPath: logDirectory.appendingPathComponent("\(baseName).\(maxBackupIndex + 1)").path))
    }

    @Test("Restarting the app starts a new dated log file and leaves the previous session untouched")
    func restartStartsNewDatedFileAndPreservesPrevious() throws {
        let fileManager = FileManager.default
        let logDirectory = fileManager.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? fileManager.removeItem(at: logDirectory) }

        let firstDate = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 0, millisecond: 0)
        let secondDate = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 5, second: 0, millisecond: 0)

        // First session, then "kill" it by releasing the writer (closes the file handle).
        try autoreleasepool {
            let writer = try LogFileWriter(
                logDirectory: logDirectory,
                date: firstDate,
                timeZone: Self.utcTimeZone,
                fileManager: fileManager
            )
            try writer.append("first session line")
        }

        // Restart: a new writer with a later launch time.
        let secondWriter = try LogFileWriter(
            logDirectory: logDirectory,
            date: secondDate,
            timeZone: Self.utcTimeZone,
            fileManager: fileManager
        )
        try secondWriter.append("second session line")

        let firstURL = logDirectory.appendingPathComponent("20260609_1200_kDrive_client.log", isDirectory: false)
        let secondURL = logDirectory.appendingPathComponent("20260609_1205_kDrive_client.log", isDirectory: false)

        #expect(secondWriter.fileURL == secondURL)
        // The previous session's file still exists and is untouched (server compresses/trims it folder-wide).
        #expect(try String(contentsOf: firstURL, encoding: .utf8) == "first session line\n")
        #expect(try String(contentsOf: secondURL, encoding: .utf8) == "second session line\n")
    }

    @Test("Restarting within the same minute appends to the existing log file without truncating")
    func restartWithinSameMinuteAppends() throws {
        let fileManager = FileManager.default
        let logDirectory = fileManager.temporaryDirectory
            .appendingPathComponent(UUID().uuidString, isDirectory: true)
        defer { try? fileManager.removeItem(at: logDirectory) }

        let date = try Self.date(year: 2026, month: 6, day: 9, hour: 12, minute: 0, second: 0, millisecond: 0)

        try autoreleasepool {
            let writer = try LogFileWriter(
                logDirectory: logDirectory,
                date: date,
                timeZone: Self.utcTimeZone,
                fileManager: fileManager
            )
            try writer.append("before restart")
        }

        let secondWriter = try LogFileWriter(
            logDirectory: logDirectory,
            date: date,
            timeZone: Self.utcTimeZone,
            fileManager: fileManager
        )
        try secondWriter.append("after restart")

        let url = logDirectory.appendingPathComponent("20260609_1200_kDrive_client.log", isDirectory: false)
        #expect(secondWriter.fileURL == url)
        #expect(try String(contentsOf: url, encoding: .utf8) == "before restart\nafter restart\n")
    }

    @Test("Default log directory matches the macOS CommonUtility log path")
    func defaultLogDirectoryMatchesCommonUtilityPath() {
        let expectedDirectory = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("Logs", isDirectory: true)
            .appendingPathComponent("kDrive", isDirectory: true)

        #expect(LogFileWriter.defaultLogDirectory() == expectedDirectory)
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
