/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2025 Infomaniak Network SA
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

namespace KDC {

void WindowsUpdater::onUpdateFound() {
    // Check if version is already downloaded
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }

    const auto expectedSize = getExpectedInstallerSize(versionInfo(_currentChannel).downloadUrl);

    // Check if an installer is already downloaded and get its size.
    uint64_t localSize = 0;
    auto ioError = IoError::Success;
    if (!IoHelper::getFileSize(filepath, localSize, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in IoHelper::getFileSize for " << Utility::formatSyncPath(filepath));
    }
    if (ioError == IoError::Success && localSize == static_cast<uint64_t>(expectedSize)) {
        LOGW_INFO(Log::instance()->getLogger(), L"Installer already downloaded at " << Utility::formatSyncPath(filepath)
                                                                                    << L". Update is ready to be installed.");
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

    const auto job = std::make_shared<DirectDownloadJob>(filepath, versionInfo(_currentChannel).downloadUrl);
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
        ss << downloadJob->errorCode() << " - " << downloadJob->errorDescr();
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
    if (!std::filesystem::exists(filepath)) {
        const auto error = "Installer file not found.";
        sentry::Handler::captureMessage(sentry::Level::Warning, "WindowsUpdater::downloadFinished", error);
        LOG_ERROR(Log::instance()->getLogger(), error);
        setState(UpdateState::DownloadError);
        return;
    }

    LOGW_INFO(Log::instance()->getLogger(),
              L"Installer downloaded at: " << Utility::formatSyncPath(filepath) << L". Update is ready to be installed.");
    setState(UpdateState::Ready);
}

bool WindowsUpdater::getInstallerPath(SyncPath &path) const {
    const auto url = versionInfo(_currentChannel).downloadUrl;
    const auto pos = url.find_last_of('/');
    const auto installerName = url.substr(pos + 1);
    SyncPath tmpDirPath;
    if (IoError ioError = IoError::Unknown; !IoHelper::tempDirectoryPath(tmpDirPath, ioError)) {
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

} // namespace KDC
