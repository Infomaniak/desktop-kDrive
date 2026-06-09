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

protocol LogFileWriting: AnyObject {
    func append(_ line: String) throws
}

final class LogFileWriter: LogFileWriting {
    private static let appName = "kDrive"
    private static let clientLogName = "kDrive_client"

    let fileURL: URL

    private let fileHandle: FileHandle

    init(
        logDirectory: URL = LogFileWriter.defaultLogDirectory(),
        date: Date = Date(),
        timeZone: TimeZone = .current,
        fileManager: FileManager = .default
    ) throws {
        try fileManager.createDirectory(at: logDirectory, withIntermediateDirectories: true)

        fileURL = Self.nextLogFileURL(in: logDirectory, date: date, timeZone: timeZone, fileManager: fileManager)
        if !fileManager.fileExists(atPath: fileURL.path) {
            _ = fileManager.createFile(atPath: fileURL.path, contents: nil)
        }

        fileHandle = try FileHandle(forWritingTo: fileURL)
        try fileHandle.seekToEnd()
    }

    deinit {
        try? fileHandle.close()
    }

    func append(_ line: String) throws {
        let data = Data("\(line)\n".utf8)

        try fileHandle.seekToEnd()
        try fileHandle.write(contentsOf: data)
    }

    static func defaultLogDirectory() -> URL {
        let libraryDirectory = FileManager.default.urls(for: .libraryDirectory, in: .userDomainMask).first
            ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library", isDirectory: true)

        return libraryDirectory
            .appendingPathComponent("Logs", isDirectory: true)
            .appendingPathComponent(appName, isDirectory: true)
    }

    private static func nextLogFileURL(in logDirectory: URL, date: Date, timeZone: TimeZone, fileManager: FileManager) -> URL {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = timeZone
        formatter.dateFormat = "yyyyMMdd_HHmm"

        let baseFileName = "\(formatter.string(from: date))_\(clientLogName).log"
        var index = 0
        var fileURL: URL

        repeat {
            fileURL = logDirectory.appendingPathComponent("\(baseFileName).\(index)")
            index += 1
        } while fileManager.fileExists(atPath: fileURL.path)

        return fileURL
    }
}
