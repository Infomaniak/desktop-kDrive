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
import OSLog

public final class LogService: @unchecked Sendable {
    public static let shared = LogService()

    private let queue = DispatchQueue(label: "com.infomaniak.kdrive.log-service")
    private let queueKey = DispatchSpecificKey<Void>()
    private let formatter: LogLineFormatter
    private let fileWriter: LogFileWriting?
    private let sentryReporter: SentryLogReporting
    private let dateProvider: () -> Date
    private let threadIDProvider: () -> String

    convenience init() {
        self.init(fileWriter: try? LogFileWriter())
    }

    init(
        formatter: LogLineFormatter = LogLineFormatter(),
        fileWriter: LogFileWriting?,
        sentryReporter: SentryLogReporting = SentryLogReporter(),
        dateProvider: @escaping () -> Date = Date.init,
        threadIDProvider: @escaping () -> String = LogService.currentThreadID
    ) {
        self.formatter = formatter
        self.fileWriter = fileWriter
        self.sentryReporter = sentryReporter
        self.dateProvider = dateProvider
        self.threadIDProvider = threadIDProvider

        queue.setSpecific(key: queueKey, value: ())
    }

    public func log(
        level: LogLevel,
        category: String,
        message: String,
        file: StaticString = #fileID,
        line: UInt = #line
    ) {
        let event = LogEvent(
            date: dateProvider(),
            level: level,
            category: category,
            threadID: threadIDProvider(),
            file: Self.fileName(from: file),
            line: line,
            message: Self.normalizedMessage(message)
        )

        sentryReporter.addBreadcrumb(event)
        if event.level >= .error {
            sentryReporter.capture(event)
        }

        queue.async { [weak self] in
            self?.write(event)
        }
    }

    func flush() {
        guard DispatchQueue.getSpecific(key: queueKey) == nil else { return }
        queue.sync {}
    }

    private func write(_ event: LogEvent) {
        guard let fileWriter else { return }

        do {
            try fileWriter.append(formatter.format(event))
        } catch {
            os_log(.error, "Failed to write kDrive log line: %@", error.localizedDescription)
        }
    }

    private static func fileName(from file: StaticString) -> String {
        let path = String(describing: file)
        return path.split(separator: "/").last.map(String.init) ?? path
    }

    private static func normalizedMessage(_ message: String) -> String {
        message
            .replacingOccurrences(of: "\r", with: "\\r")
            .replacingOccurrences(of: "\n", with: "\\n")
    }

    private static func currentThreadID() -> String {
        var threadID: UInt64 = 0
        pthread_threadid_np(nil, &threadID)
        return String(threadID)
    }
}
