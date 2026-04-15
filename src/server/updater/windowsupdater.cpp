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

#include <QProcess>
#include "windowsupdater.h"
#include "log/log.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/syncjobmanager.h"
#include "io/iohelper.h"
#include "libcommonserver/utility/utility.h" // Path2WStr
#include "utility/digitalsignaturechecker_win.h"

namespace KDC {

void WindowsUpdater::onUpdateFound() {
    // Check if version is already downloaded
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }

    const auto expectedSize = getExpectedInstallerSize(versionInfo().downloadUrl);

    // Check if an installer is already downloaded and get its size.
    uint64_t localSize = 0;
    auto ioError = IoError::Success;
    if (!IoHelper::getFileSize(filepath, localSize, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::getFileSize for " << Utility::formatSyncPath(filepath));
    }
    if (ioError == IoError::Success && localSize == static_cast<uint64_t>(expectedSize)) {
        LOGW_INFO(Log::instance()->getLogger(), L"Installer already downloaded at " << Utility::formatSyncPath(filepath)
                                                                                    << L". Update is ready to be installed.");

        if (!verifyDigitalSignature(filepath)) {
            setState(UpdateState::UpdateError);
            return;
        }

        setState(UpdateState::Ready);
        return;
    }

    downloadUpdate();
}

void WindowsUpdater::startInstaller() {
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }
    if (std::error_code ec; !std::filesystem::exists(filepath, ec)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Installer file not found. " << Utility::formatStdError(filepath, ec));
        if (_autoUpdate) {
            LOG_ERROR(Log::instance()->getLogger(), "Already tried to re-download the installer before.");
            setState(UpdateState::UpdateError);
            return;
        }
        _autoUpdate = true;
        downloadUpdate();
        return;
    }
    if (!verifyFileChecksum(filepath)) {
        setState(UpdateState::UpdateError); // Or create UpdateState::ChecksumError
        return;
    }

    if (!verifyDigitalSignature(filepath)) {
        setState(UpdateState::UpdateError);
        return;
    }

    _autoUpdate = false;

    LOGW_INFO(Log::instance()->getLogger(), L"Starting updater " << Utility::formatSyncPath(filepath));
    const auto cmd = filepath.wstring() + L" /S /launch";
    (void) Utility::runDetachedProcess(cmd);
}

void WindowsUpdater::downloadUpdate() noexcept {
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }

    // Remove an eventual already existing installer file.
    auto ioError = IoError::Success;
    (void) IoHelper::deleteItem(filepath, ioError);
    if (ioError != IoError::Success && ioError != IoError::NoSuchFileOrDirectory) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to to remove existing installer " << Utility::formatSyncPath(filepath));
    }

    const auto job = std::make_shared<DirectDownloadJob>(filepath, versionInfo().downloadUrl);
    const std::function<void(UniqueId)> callback = std::bind_front(&WindowsUpdater::downloadFinished, this);
    job->setAdditionalCallback(callback);
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
    setState(UpdateState::Downloading);
}

void WindowsUpdater::downloadFinished(const UniqueId jobId) {
    const auto job = SyncJobManagerSingleton::instance()->getJob(jobId);
    const auto downloadJob = std::dynamic_pointer_cast<DirectDownloadJob>(job);
    if (!downloadJob) {
        const auto error = "Could not cast job pointer.";
        sentry::Handler::captureMessage(sentry::Level::Warning, "WindowsUpdater::downloadFinished", error);
        LOG_ERROR(Log::instance()->getLogger(), error);
        setState(UpdateState::DownloadError);
        assert(false);
        return;
    }

    if (downloadJob->hasErrorApi()) {
        std::stringstream ss;
        ss << downloadJob->backError().code() << " - " << downloadJob->backError().description();
        sentry::Handler::captureMessage(sentry::Level::Warning, "WindowsUpdater::downloadFinished", ss.str());
        LOG_ERROR(Log::instance()->getLogger(), ss.str());
        setState(UpdateState::DownloadError);
        return;
    }

    // Verify that the installer is present on local filesystem
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }

    if (std::error_code ec; !std::filesystem::exists(filepath, ec)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Installer file not found. " << Utility::formatStdError(filepath, ec));
        downloadUpdate();
        return;
    }

    LOGW_INFO(Log::instance()->getLogger(),
              L"Installer downloaded at: " << Utility::formatSyncPath(filepath) << L". Update is ready to be installed.");
    setState(UpdateState::Ready);
    if (_autoUpdate) {
        // Start the installer automatically.
        startInstaller();
    }
}

bool WindowsUpdater::getInstallerPath(SyncPath &path) const {
    const auto url = versionInfo().downloadUrl;
    const auto pos = url.find_last_of('/');
    const auto installerName = url.substr(pos + 1);
    SyncPath tmpDirPath;
    if (IoError ioError = IoError::Unknown; !IoHelper::deviceTempDirectoryPath(tmpDirPath, ioError)) {
        sentry::Handler::captureMessage(sentry::Level::Warning, "WindowsUpdater::getInstallerPath",
                                        "Impossible to retrieve installer destination directory.");
        return false;
    }
    path = tmpDirPath / installerName;
    return true;
}

std::streamsize WindowsUpdater::getExpectedInstallerSize(const std::string &downloadUrl) {
    // Get the expected size of the installer.
    DirectDownloadJob job(downloadUrl);
    (void) job.runSynchronously();
    return job.httpResponse().getContentLength();
}

bool WindowsUpdater::verifyFileChecksum(const SyncPath &filepath) {
    // const auto &expectedChecksum = versionInfo(_currentChannel).checksum;
    const std::string &expectedChecksum = "083a301369cd711e9803f7d90d342a3778f9cb864ab22992b49fccddc3b9256c"; // capybara test

    if (expectedChecksum.empty()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Checksum not available from API. Skipping verification.");
        return true;    // to not be blocking as long as not implemented on kStore
    }

    std::string actualChecksum = computeFileChecksum(filepath);

    if (actualChecksum.empty()) {
        LOGW_WARN(Log::instance()->getLogger(), L"Failed to compute file checksum.");
        return false; // block the installation
    }
    if (actualChecksum != expectedChecksum) {
        LOGW_ERROR(Log::instance()->getLogger(), L"Checksum mismatch! Expected: " << CommonUtility::s2ws(expectedChecksum)
                                                                                  << L", Got: "
                                                                                  << CommonUtility::s2ws(actualChecksum));
        return false;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Checksum verification passed.");
    return true;
}

std::string WindowsUpdater::computeFileChecksum(const SyncPath &filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        LOGW_WARN(Log::instance()->getLogger(), L"missing filepath for Checksum compute");
        return "";
    }

    SHA256_CTX sha256;  // Using SHA256 instead of the project-standard XXH3 for security.
                        // XXH3 is a non-cryptographic hash; an attacker could craft a malicious
                        // file with the same XXH3 hash (collision attack).
    SHA256_Init(&sha256);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    // Process remaining bytes
    SHA256_Update(&sha256, buffer, file.gcount());

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int) hash[i];
    }

    return ss.str();
}

bool WindowsUpdater::verifyDigitalSignature(const SyncPath &filepath) {
    if (!DigitalSignatureChecker_win(filepath).isSignatureValid()) {
        const auto error =
                L"The digital signature of installer " + Utility::formatSyncPath(filepath) + L" is invalid. Aborting update.";
        sentry::Handler::captureMessage(sentry::Level::Error, "Invalid signature", CommonUtility::ws2s(error));
        LOGW_ERROR(Log::instance()->getLogger(), error);
        auto ioError = IoError::Success;
        (void) IoHelper::deleteItem(filepath, ioError);
        return false;
    }
    return true;
}

} // namespace KDC
