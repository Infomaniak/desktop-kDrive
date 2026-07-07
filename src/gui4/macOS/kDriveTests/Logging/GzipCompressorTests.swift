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

@Suite("GzipCompressor Tests")
struct GzipCompressorTests {
    @Test("Compresses then decompresses a file back to its original content")
    func roundTripsFileThroughGzip() throws {
        let fileManager = FileManager.default
        let directory = fileManager.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? fileManager.removeItem(at: directory) }

        let originalURL = directory.appendingPathComponent("original.log", isDirectory: false)
        let compressedURL = directory.appendingPathComponent("original.log.gz", isDirectory: false)
        let decompressedURL = directory.appendingPathComponent("decompressed.log", isDirectory: false)

        // Large enough (~1.5 MiB) to exercise multi-chunk streaming, with multibyte UTF-8 content.
        let original = String(repeating: "kDrive log line with some entropy 0123456789 éàü\n", count: 30000)
        try original.write(to: originalURL, atomically: true, encoding: .utf8)

        #expect(GzipCompressor.compress(source: originalURL, destination: compressedURL))

        // The output is a real gzip file (magic 0x1f 0x8b) and smaller than the source.
        let compressedData = try Data(contentsOf: compressedURL)
        let originalData = try Data(contentsOf: originalURL)
        #expect(Array(compressedData.prefix(2)) == [0x1F, 0x8B])
        #expect(compressedData.count < originalData.count)

        #expect(GzipCompressor.decompress(source: compressedURL, destination: decompressedURL))

        let decompressed = try String(contentsOf: decompressedURL, encoding: .utf8)
        #expect(decompressed == original)
    }

    @Test("Round-trips an empty file")
    func roundTripsEmptyFile() throws {
        let fileManager = FileManager.default
        let directory = fileManager.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? fileManager.removeItem(at: directory) }

        let originalURL = directory.appendingPathComponent("empty.log", isDirectory: false)
        let compressedURL = directory.appendingPathComponent("empty.log.gz", isDirectory: false)
        let decompressedURL = directory.appendingPathComponent("empty-decompressed.log", isDirectory: false)

        #expect(fileManager.createFile(atPath: originalURL.path, contents: nil))

        #expect(GzipCompressor.compress(source: originalURL, destination: compressedURL))
        #expect(try Array(Data(contentsOf: compressedURL).prefix(2)) == [0x1F, 0x8B])

        #expect(GzipCompressor.decompress(source: compressedURL, destination: decompressedURL))
        #expect(try Data(contentsOf: decompressedURL).isEmpty)
    }

    @Test("Compressing a missing source fails")
    func compressMissingSourceFails() {
        let fileManager = FileManager.default
        let directory = fileManager.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)

        let missingURL = directory.appendingPathComponent("does-not-exist.log", isDirectory: false)
        let compressedURL = directory.appendingPathComponent("does-not-exist.log.gz", isDirectory: false)

        #expect(!GzipCompressor.compress(source: missingURL, destination: compressedURL))
    }
}
