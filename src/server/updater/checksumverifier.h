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

#include "utility/types.h"

#include <string>

namespace KDC {

/**
 * @brief Verifies the integrity of a downloaded installer by comparing its SHA-256 checksum
 *        against the expected value fetched from a `.sha256` sidecar file.
 *
 * The checksum verification is mandatory and blocking: if the sidecar file is missing,
 * empty, or the computed checksum does not match, the installer file is deleted and the
 * update is prevented.
 */
class ChecksumVerifier {
    public:
        ChecksumVerifier() = default;
        virtual ~ChecksumVerifier() = default;

        /**
         * @brief Verify the checksum of the downloaded installer against the `.sha256` sidecar file.
         *
         * Derives the sidecar URL from `downloadUrl` by inserting `.sha256` before any query string
         * or fragment, downloads it, computes the local file's SHA-256, and compares.
         *
         * On failure the installer file is deleted, a Sentry event is captured, and `false` is returned.
         *
         * @param filepath    Path to the downloaded installer file.
         * @param downloadUrl The original download URL of the installer (used to derive the `.sha256` sidecar URL).
         * @return true if the checksum is valid; false if the sidecar file is unavailable, empty, or the checksum
         *         does not match.
         */
        bool verifyFileChecksum(const SyncPath &filepath, const std::string &downloadUrl);

        /**
         * @brief Compute the SHA-256 checksum of a file.
         * @param filepath Path to the file.
         * @return The hex-encoded SHA-256 digest, or an empty string on failure.
         */
        static std::string computeFileChecksum(const SyncPath &filepath);

    private:
        /**
         * @brief Download the `.sha256` sidecar file and extract the expected checksum (first whitespace-delimited
         *        token of the file content).
         *
         * Virtual so that tests can override it without hitting the network.
         *
         * @param sha256Url   URL of the `.sha256` sidecar file.
         * @param outChecksum The expected checksum (hex string) if download succeeded.
         * @return true if the file was downloaded successfully and contained a non-empty checksum.
         */
        virtual bool downloadSha256File(const std::string &sha256Url, std::string &outChecksum);
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
