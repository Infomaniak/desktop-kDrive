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

/// Size-based rolling log file writer for the macOS app.
///
/// Logs are written to an active `<date>_kDrive_client.log` file, where `<date>` is the session
/// creation time (mirroring the server's `<date>_kDrive.log` naming). When it grows past
/// `maxFileSize` it is rotated: the active file becomes `<date>_kDrive_client.log.0.gz` and the
/// previous backups are shifted up (`.0` → `.1`, `.1` → `.2`, …). Backups beyond `maxBackupIndex`
/// are removed. This mirrors the C++ `CustomRollingFileAppender` (500 MiB, gzip-compressed backups).
final class LogFileWriter: LogFileWriting {
    private static let clientLogName = "kDrive_client"
    /// Creation-time prefix format, matching the server's `LOGFILE_TIME_FORMAT` (`%Y%m%d_%H%M`).
    private static let dateFormat = "yyyyMMdd_HHmm"

    /// 500 MiB, matching the C++ `CommonUtility::logMaxSize`.
    static let defaultMaxFileSize: UInt64 = 500 * 1024 * 1024
    /// Highest backup index kept on disk. Backups range from `.0` to `.maxBackupIndex` (gzip-compressed).
    static let defaultMaxBackupIndex = 4

    let fileURL: URL

    private let logDirectory: URL
    private let baseFileName: String
    private let fileManager: FileManager
    private let maxFileSize: UInt64
    private let maxBackupIndex: Int

    private var fileHandle: FileHandle

    init(
        logDirectory: URL = LogFileWriter.defaultLogDirectory(),
        date: Date = Date(),
        timeZone: TimeZone = .current,
        fileManager: FileManager = .default,
        maxFileSize: UInt64 = LogFileWriter.defaultMaxFileSize,
        maxBackupIndex: Int = LogFileWriter.defaultMaxBackupIndex
    ) throws {
        self.logDirectory = logDirectory
        self.fileManager = fileManager
        self.maxFileSize = maxFileSize
        self.maxBackupIndex = maxBackupIndex

        try fileManager.createDirectory(at: logDirectory, withIntermediateDirectories: true)

        baseFileName = Self.makeBaseFileName(date: date, timeZone: timeZone)
        fileURL = logDirectory.appendingPathComponent(baseFileName, isDirectory: false)
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

        let currentSize = try fileHandle.seekToEnd()
        if currentSize > maxFileSize {
            try rotate()
        }

        try fileHandle.seekToEnd()
        try fileHandle.write(contentsOf: data)
    }

    static func defaultLogDirectory() -> URL {
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library", isDirectory: true)
            .appendingPathComponent("Logs", isDirectory: true)
            .appendingPathComponent(Constants.appName, isDirectory: true)
    }

    private static func makeBaseFileName(date: Date, timeZone: TimeZone) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = timeZone
        formatter.dateFormat = dateFormat

        return "\(formatter.string(from: date))_\(clientLogName).log"
    }

    // MARK: - Rotation

    private func rotate() throws {
        try fileHandle.close()

        shiftBackups()

        // Move the active file to `.0`, then compress it to `.0.gz`.
        let rotatedURL = backupURL(index: 0, compressed: false)
        let compressedURL = backupURL(index: 0, compressed: true)
        removeIfExists(rotatedURL)
        removeIfExists(compressedURL)
        try fileManager.moveItem(at: fileURL, to: rotatedURL)

        if GzipCompressor.compress(source: rotatedURL, destination: compressedURL) {
            removeIfExists(rotatedURL)
        } else {
            // Compression failed: keep the uncompressed rotated file rather than losing logs.
            removeIfExists(compressedURL)
        }

        // Start a fresh active log file.
        _ = fileManager.createFile(atPath: fileURL.path, contents: nil)
        fileHandle = try FileHandle(forWritingTo: fileURL)
        try fileHandle.seekToEnd()
    }

    /// Deletes the oldest backup and shifts every other backup up by one index.
    private func shiftBackups() {
        // Delete the oldest backup, which would otherwise overflow past `maxBackupIndex`.
        removeIfExists(backupURL(index: maxBackupIndex, compressed: true))
        removeIfExists(backupURL(index: maxBackupIndex, compressed: false))

        // Shift `.i` -> `.(i + 1)` from the second-oldest down to the newest.
        for index in stride(from: maxBackupIndex - 1, through: 0, by: -1) {
            moveBackup(from: index, to: index + 1, compressed: true)
            moveBackup(from: index, to: index + 1, compressed: false)
        }
    }

    private func moveBackup(from: Int, to: Int, compressed: Bool) {
        let source = backupURL(index: from, compressed: compressed)
        guard fileManager.fileExists(atPath: source.path) else { return }

        let destination = backupURL(index: to, compressed: compressed)
        removeIfExists(destination)
        try? fileManager.moveItem(at: source, to: destination)
    }

    private func backupURL(index: Int, compressed: Bool) -> URL {
        let suffix = compressed ? ".gz" : ""
        return logDirectory.appendingPathComponent("\(baseFileName).\(index)\(suffix)", isDirectory: false)
    }

    private func removeIfExists(_ url: URL) {
        guard fileManager.fileExists(atPath: url.path) else { return }
        try? fileManager.removeItem(at: url)
    }
}
