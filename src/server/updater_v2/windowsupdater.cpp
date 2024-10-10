/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "windowsupdater.h"
#include "libcommon/utility/utility.h"
#include "jobs/network/directdownloadjob.h"
#include "jobs/jobmanager.h"
#include "log/sentry/sentryhandler.h"
#include "log/log.h"
#include "io/iohelper.h"
#include "utility/utility.h"

namespace KDC {

void WindowsUpdater::onUpdateFound() {
    // Check if version is already downloaded
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateStateV2::DownloadError);
        return;
    }
    if (std::filesystem::exists(filepath)) {
        LOG_INFO(Log::instance()->getLogger(),
                 L"Installer already downloaded at: " << Path2WStr(filepath).c_str() << L". Update is ready to be installed.");
        setState(UpdateStateV2::Ready);
        return;
    }

    downloadUpdate();
}

void WindowsUpdater::startInstaller() {
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateStateV2::DownloadError);
        return;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Starting updater " << Path2WStr(filepath).c_str());

    auto *updaterThread = new std::jthread([filepath] {
        const auto cmd = filepath.string() + " /S /launch";
        std::system(cmd.c_str());
    });
    updaterThread->detach();
}

void WindowsUpdater::downloadUpdate() noexcept {
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateStateV2::DownloadError);
        return;
    }

    auto job = std::make_shared<DirectDownloadJob>(filepath, versionInfo().downloadUrl);
    const std::function<void(UniqueId)> callback = std::bind_front(&WindowsUpdater::downloadFinished, this);
    JobManager::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL, callback);
    setState(UpdateStateV2::Downloading);
}

void WindowsUpdater::downloadFinished(UniqueId jobId) {
    auto job = JobManager::instance()->getJob(jobId);
    const auto downloadJob = std::dynamic_pointer_cast<DirectDownloadJob>(job);
    if (!downloadJob) {
        const auto error = "Could not cast job pointer.";
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "WindowsUpdater::downloadFinished", error);
        LOG_ERROR(Log::instance()->getLogger(), error);
        setState(UpdateStateV2::DownloadError);
        return;
    }

    std::string errorCode;
    std::string errorDescr;
    if (downloadJob->hasErrorApi(&errorCode, &errorDescr)) {
        std::stringstream ss;
        ss << errorCode.c_str() << " - " << errorDescr;
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "WindowsUpdater::downloadFinished", ss.str());
        LOG_ERROR(Log::instance()->getLogger(), ss.str().c_str());
        setState(UpdateStateV2::DownloadError);
        return;
    }

    // Verify that the installer is present on local filesystem
    SyncPath filepath;
    if (!getInstallerPath(filepath)) {
        setState(UpdateStateV2::DownloadError);
        return;
    }
    if (!std::filesystem::exists(filepath)) {
        const auto error = "Installer file not found.";
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "WindowsUpdater::downloadFinished", error);
        LOG_ERROR(Log::instance()->getLogger(), error);
        setState(UpdateStateV2::DownloadError);
        return;
    }

    LOG_INFO(Log::instance()->getLogger(),
             L"Installer downloaded at: " << Path2WStr(filepath).c_str() << L". Update is ready to be installed.");
    setState(UpdateStateV2::Ready);
}

bool WindowsUpdater::getInstallerPath(SyncPath &path) const {
    const auto pos = versionInfo().downloadUrl.find_last_of('/');
    const auto installerName = versionInfo().downloadUrl.substr(pos + 1);
    SyncPath tmpDirPath;
    if (IoError ioError = IoError::Unknown; !IoHelper::tempDirectoryPath(tmpDirPath, ioError)) {
        SentryHandler::instance()->captureMessage(SentryLevel::Warning, "WindowsUpdater::getInstallerPath",
                                                  "Impossible to retrieve installer destination directory.");
        return false;
    }
    path = tmpDirPath / installerName;
    return true;
}

} // namespace KDC
