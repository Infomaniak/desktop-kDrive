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
    if (std::filesystem::exists(filepath)) {
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
    auto cmd = filepath.wstring() + L" /S /launch";
    Utility::runDetachedProcess(cmd);
}

void WindowsUpdater::downloadUpdate() noexcept {
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateState::DownloadError);
        return;
    }

    auto job = std::make_shared<DirectDownloadJob>(filepath, versionInfo(_currentChannel).downloadUrl);
    const std::function<void(UniqueId)> callback = std::bind_front(&WindowsUpdater::downloadFinished, this);
    job->setAdditionalCallback(callback);
    SyncJobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
    setState(UpdateState::Downloading);
}

void WindowsUpdater::downloadFinished(const UniqueId jobId) {
    auto job = SyncJobManager::instance()->getJob(jobId);
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

} // namespace KDC
