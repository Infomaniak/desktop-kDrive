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
import zlib

/// Streaming gzip (`.gz`) compressor/decompressor backed by the system `zlib`.
///
/// Mirrors the C++ `CommonUtility::compressFile` (`gzopen` / `gzwrite` / `gzclose`) so the
/// compressed log backups produced by the macOS app are byte-compatible with the rest of the
/// kDrive tooling and decompressable with `gunzip`.
enum GzipCompressor {
    /// Read/write chunk size, matching the C++ implementation (1 MiB).
    private static let chunkSize = 1024 * 1024

    /// Compresses `source` into a gzip file at `destination`, streaming in chunks so files of any
    /// size are never loaded into memory at once.
    ///
    /// - Returns: `true` on success. On failure the caller is responsible for cleaning up any
    ///   partially written `destination`.
    static func compress(source: URL, destination: URL) -> Bool {
        guard let input = try? FileHandle(forReadingFrom: source) else { return false }
        defer { try? input.close() }

        let handle = destination.path.withCString { pathPtr in
            "wb".withCString { modePtr in
                gzopen(pathPtr, modePtr)
            }
        }
        guard let handle else { return false }

        var success = true
        while true {
            let chunk: Data
            do {
                chunk = try input.read(upToCount: chunkSize) ?? Data()
            } catch {
                success = false
                break
            }
            if chunk.isEmpty { break }

            let written = chunk.withUnsafeBytes { rawBuffer in
                gzwrite(handle, rawBuffer.baseAddress, UInt32(rawBuffer.count))
            }
            if written <= 0 || Int(written) != chunk.count {
                success = false
                break
            }
        }

        let closeResult = gzclose(handle)
        return success && closeResult == Z_OK
    }

    // periphery:ignore - Counterpart to compress, used to read gzip-archived log backups.
    static func decompress(source: URL, destination: URL) -> Bool {
        let handle = source.path.withCString { pathPtr in
            "rb".withCString { modePtr in
                gzopen(pathPtr, modePtr)
            }
        }
        guard let handle else { return false }

        _ = FileManager.default.createFile(atPath: destination.path, contents: nil)
        guard let output = try? FileHandle(forWritingTo: destination) else {
            _ = gzclose(handle)
            return false
        }
        defer { try? output.close() }

        var success = true
        var buffer = [UInt8](repeating: 0, count: chunkSize)
        while true {
            let bytesRead = buffer.withUnsafeMutableBytes { rawBuffer in
                gzread(handle, rawBuffer.baseAddress, UInt32(rawBuffer.count))
            }
            if bytesRead < 0 {
                success = false
                break
            }
            if bytesRead == 0 { break }

            do {
                try output.write(contentsOf: Data(buffer.prefix(Int(bytesRead))))
            } catch {
                success = false
                break
            }
        }

        let closeResult = gzclose(handle)
        return success && closeResult == Z_OK
    }
}
