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

#pragma once

#include "libcommon/utility/types.h"

#include <functional>
#include <string>

namespace KDC {

/**
 * @brief Static helper class for verifying the integrity of downloaded installer files via SHA-256
 *        checksums fetched from `.sha256` sidecar files.
 *
 * The checksum verification is mandatory and blocking: if the sidecar file is missing, empty, or the
 * computed checksum does not match, the installer file is deleted and the update is prevented.
 *
 * The download mechanism is injected as a lambda (`Sha256Fetcher`) so that callers can use their own
 * HTTP client (e.g. `DirectDownloadJob` in the server, `HttpDownloader` in the recovery updater).
 */
class ChecksumVerifier {
    public:
        /**
         * @brief Lambda type for downloading the `.sha256` sidecar file content.
         *
         * The implementation should download the file at `url`, extract the first whitespace-delimited
         * token (the checksum), and return it. Return an empty string on failure.
         *
         * @param url The URL of the `.sha256` sidecar file.
         * @return The expected checksum (hex string), or an empty string on failure.
         */
        using Sha256Fetcher = std::function<std::string(const std::string &url)>;

        /**
         * @brief Verify the checksum of the downloaded installer against the `.sha256` sidecar file.
         *
         * Derives the sidecar URL from `downloadUrl`, fetches it via `fetcher`, computes the local
         * file's SHA-256, and compares.
         *
         * On failure the installer file is deleted, a Sentry event is captured, and `false` is returned.
         *
         * @param filepath    Path to the downloaded installer file.
         * @param downloadUrl The original download URL of the installer (used to derive the `.sha256` sidecar URL).
         * @param fetcher     Lambda that downloads the `.sha256` sidecar file and returns the expected checksum.
         * @param outError    Optional. If provided, filled with a specific error message on failure
         *                    ("sha256FileUnavailable", "computeFailed", or "mismatch").
         * @return true if the checksum is valid; false if the sidecar file is unavailable, empty, or the
         *         checksum does not match.
         */
        static bool verifyFileChecksum(const SyncPath &filepath, const std::string &downloadUrl, const Sha256Fetcher &fetcher);

        /**
         * @brief Compute the SHA-256 checksum of a file.
         * @param filepath Path to the file.
         * @return The hex-encoded SHA-256 digest, or an empty string on failure.
         */
        static std::string computeFileChecksum(const SyncPath &filepath);

        /**
         * @brief Derive the `.sha256` sidecar URL from the installer download URL.
         *
         * Inserts `.sha256` before any query string (`?`) or fragment (`#`) so that signed/CDN URLs
         * keep their sidecar resolvable.
         *
         * @param downloadUrl The installer download URL.
         * @return The `.sha256` sidecar URL.
         */
        static std::string buildSha256Url(const std::string &downloadUrl);
};

} // namespace KDC
