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

#include "checksumverifier.h"

#include "libcommon/utility/utility.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <Poco/SHA2Engine.h>
#include <Poco/DigestEngine.h>

#include <array>
#include <fstream>

namespace KDC {

std::string ChecksumVerifier::buildSha256Url(const std::string &downloadUrl) {
    const auto suffixPos = downloadUrl.find_first_of("?#");
    return downloadUrl.substr(0, suffixPos) + ".sha256" +
           (suffixPos == std::string::npos ? std::string{} : downloadUrl.substr(suffixPos));
}

bool ChecksumVerifier::verifyFileChecksum(const SyncPath &filepath, const std::string &downloadUrl,
                                          const Sha256Fetcher &fetcher) {
    auto cleanupAndFail = [&](const std::string &reason) {
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(filepath, ioError);
        if (ioError == IoError::Success) {
            LOGW_INFO(Log::instance()->getLogger(), L"corrupted file at " << Utility::formatSyncPath(filepath) << L" deleted");
        } else {
            LOGW_WARN(Log::instance()->getLogger(), L"couldn't reach corrupted file at " << Utility::formatSyncPath(filepath)
                                                                                         << L" : IOError state "
                                                                                         << static_cast<int>(ioError));
        }

        // Send to Sentry
        sentry::Handler::captureMessage(sentry::Level::Error, "ChecksumVerifier::verifyFileChecksum",
                                        "Checksum verification failed: " + reason);

        LOGW_ERROR(Log::instance()->getLogger(), L"Checksum verification failed: " << CommonUtility::s2ws(reason));
        return false;
    };

    // Derive the .sha256 sidecar URL and fetch the expected checksum — this is mandatory.
    const std::string sha256Url = buildSha256Url(downloadUrl);

    std::string expectedChecksum;
    if (fetcher) expectedChecksum = fetcher(sha256Url);
    if (expectedChecksum.empty()) {
        LOGW_ERROR(Log::instance()->getLogger(),
                   L"SHA-256 sidecar file unavailable or empty for " << CommonUtility::s2ws(downloadUrl));
        return cleanupAndFail("sha256FileUnavailable");
    }
    expectedChecksum = CommonUtility::trim(CommonUtility::toLower(expectedChecksum));

    // Compute actual checksum
    const std::string actualChecksum = CommonUtility::trim(CommonUtility::toLower(computeFileChecksum(filepath)));
    if (actualChecksum.empty()) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Failed to compute file checksum.");
        return cleanupAndFail("computeFailed");
    }

    // Verify checksum
    if (actualChecksum != expectedChecksum) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Checksum mismatch! Expected: " << CommonUtility::s2ws(expectedChecksum)
                                                                                  << L", Got: "
                                                                                  << CommonUtility::s2ws(actualChecksum));
        return cleanupAndFail("mismatch");
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Checksum verification passed.");
    return true;
}

std::string ChecksumVerifier::computeFileChecksum(const SyncPath &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return "";

    Poco::SHA2Engine sha256(Poco::SHA2Engine::ALGORITHM::SHA_256);
    // Using SHA256 instead of the project-standard XXH3 for security.
    // XXH3 is a non-cryptographic hash; an attacker could craft a malicious
    // file with the same XXH3 hash (collision attack).

    std::array<char, 8192> buffer{};
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        sha256.update(buffer.data(), static_cast<std::size_t>(file.gcount()));
    }

    return Poco::DigestEngine::digestToHex(sha256.digest());
}

} // namespace KDC
