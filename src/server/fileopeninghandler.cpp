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

#include "fileopeninghandler.h"

#include "appserver.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/urlhelper.h"
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "jobs/network/jobexceptions.h"
#include "libsyncengine/jobs/network/kDrive_API/getfileinfojob.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <QDesktopServices>
#include <QObject>

#include <Poco/File.h>

#include <log4cplus/loggingmacros.h>

#include <algorithm>
#include <functional>
#include <string_view>
#include <unordered_set>

namespace KDC {

namespace {
//! Transparent hasher enabling heterogeneous lookup in associative string containers (e.g. `contains` with a
//! `std::string_view` argument) without constructing a `std::string` for every lookup.
struct TransparentStringHash {
        using is_transparent = void;

        size_t operator()(const std::string_view str) const noexcept { return std::hash<std::string_view>{}(str); }
};
} // namespace

FileOpeningHandler::FileOpeningHandler(AppServer *const appServer, std::string url) :
    _appServer(appServer),
    _url(std::move(url)) {}

void FileOpeningHandler::queueOpenFileJob() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url);

    if (OpenFileJob::Request request; !OpenFileJob::parseDeeplinkUrl(_url, request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url);
        handleFileOpeningFailure(FileOpeningPhase::Failed);

        return;
    }

    // Reject the request when too many file-opening flows are already active: each one holds a job-pool worker while
    // it waits (for the item to be synchronized, then for its hydration), starving the dependent jobs (hydration,
    // download) that must run in the same bounded pool. Like an invalid URL, a rejected request sends no status to
    // the client and therefore opens no pop-up window: the user is informed through a notification only.
    const auto maxFlows = maxConcurrentFileOpeningJobs();
    if (const auto activeFlows = _appServer->fileOpeningJobCount(); activeFlows >= maxFlows) {
        LOG_WARN(Log::instance()->getLogger(), "Too many concurrent file opening requests - active="
                                                       << activeFlows << ", max=" << maxFlows << " - url=" << _url);
        handleFileOpeningFailure(FileOpeningPhase::Failed);

        return;
    }

    auto getSyncPal = [this](const SyncDbId syncDbId) -> std::shared_ptr<SyncPal> {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(syncDbId);

        return syncPalMapIt == _appServer->syncPalMap.end() ? nullptr : syncPalMapIt->second;
    };

    auto getVfs = [this](const SyncDbId syncDbId) -> std::shared_ptr<Vfs> {
        std::shared_ptr<Vfs> vfs;
        (void) _appServer->getVfs(syncDbId, vfs);

        return vfs;
    };

    const auto job = std::make_shared<OpenFileJob>(_url, std::move(getSyncPal), std::move(getVfs));
    _jobId = job->jobId();
    _job = job;
    _appServer->registerFileOpeningJob(_jobId, shared_from_this());
    const std::weak_ptr<FileOpeningHandler> weakThis = weak_from_this();
    job->setFeedbackCallback([weakThis, appServer = _appServer](const FileOpeningPhase phase, const int8_t percent,
                                                                const CommString &itemName, const CommString &itemPath,
                                                                const int64_t receivedBytes, const int64_t totalBytes) {
        // Job manager worker thread. A null lock means the flow has already been processed and its handler destroyed:
        // discard the call. Hand the state over to the application thread otherwise.
        if (const auto handler = weakThis.lock()) {
            appServer->dispatchFileOpeningStatus(handler, phase, percent, itemName, itemPath, receivedBytes, totalBytes);
        }
    });
    std::weak_ptr<OpenFileJob> weakJob = job;
    job->setAdditionalCallback([weakThis, appServer = _appServer, weakJob](UniqueId) {
        // Job manager worker thread. A null handler lock means the flow has already been processed and its handler
        // destroyed: discard the call. A null job lock means the job has been destroyed after completion: nothing
        // left to report. Hand the outcome over to the application thread otherwise.
        const auto handler = weakThis.lock();
        if (!handler) return;

        const auto job = weakJob.lock();
        if (!job) return;

        appServer->dispatchFileOpeningOutcome(handler, job->outcome(), job->localPath(), job->syncDbId());
    });
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void FileOpeningHandler::onOpenFileJobFinished(const OpenFileJob::Outcome outcome, const SyncPath &localPath,
                                               const SyncDbId syncDbId) {
    // Close the pop-up window on the client side, if one is open (item being synchronized or download in progress).
    // The item name is irrelevant here: the clients close the window upon receiving the Finished phase.
    _appServer->sendFileOpeningStatus(_jobId, FileOpeningPhase::Finished, -1, {}, {}, -1, -1);

    switch (outcome) {
        case OpenFileJob::Outcome::ReadyToOpen:
            (void) openLocalItem(localPath, syncDbId);
            break;
        case OpenFileJob::Outcome::NotFound:
            handleFileOpeningFailure(FileOpeningPhase::NotFound);
            break;
        case OpenFileJob::Outcome::ResolveTimedOut:
            handleFileOpeningFailure(FileOpeningPhase::Failed);
            break;
        case OpenFileJob::Outcome::HydrationTimedOut:
            handleFileOpeningFailure(FileOpeningPhase::Failed);
            break;
        case OpenFileJob::Outcome::Failed:
            handleFileOpeningFailure(FileOpeningPhase::Failed);
            break;
        case OpenFileJob::Outcome::Canceled:
            handleFileOpeningCanceled();
            break;
    }
}

ExitInfo FileOpeningHandler::openLocalItem(const SyncPath &localPath, const SyncDbId syncDbId) {
    SyncPath pathToOpen = localPath;
    if (shouldOpenParentFolder(localPath)) {
        LOGW_INFO(Log::instance()->getLogger(),
                  L"Executable item, revealing its parent folder instead - " << Utility::formatSyncPath(localPath));
        pathToOpen = localPath.parent_path();
    }

    bool isDeHydrated = false;
    if (auto ioError = IoError::Success; !IoHelper::checkIfFileIsDehydrated(pathToOpen, isDeHydrated, ioError)) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in IoHelper::checkIfFileIsDehydrated - " << Utility::formatIoError(pathToOpen, ioError));

        handleFileOpeningFailure(FileOpeningPhase::Failed);

        return ExitCode::SystemError;
    }

    // Do not attempt to open a dehydrated placeholder if the engine of the sync is not running: the system is unable
    // to hydrate it.
    if (isDeHydrated && !isSyncEngineRunning(syncDbId)) {
        handleFileOpeningFailure(FileOpeningPhase::Failed);

        return ExitCode::DataError;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        handleFileOpeningFailure(FileOpeningPhase::Failed);

        return ExitCode::SystemError;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Item opened from URL - " << Utility::formatSyncPath(pathToOpen));
    handleFileOpeningSuccess();

    return ExitCode::Ok;
}

bool FileOpeningHandler::isSyncEngineRunning(const SyncDbId syncDbId) const {
    const std::scoped_lock lock(_appServer->syncPalMapMutex);
    const auto it = _appServer->syncPalMap.find(syncDbId);
    return it != _appServer->syncPalMap.end() && it->second && it->second->isRunning();
}

void FileOpeningHandler::handleFileOpeningSuccess() {
    _appServer->unregisterFileOpeningJob(_jobId);
}

void FileOpeningHandler::handleFileOpeningFailure(const FileOpeningPhase failurePhase) {
    LOG_WARN(Log::instance()->getLogger(), "Failed to open item from deeplink URL - url=" << _url);

    // Inform the client interface of the failure: the pop-up window of the request is replaced by a failure window.
    // No notification is sent anymore: the failure window replaces it. The web address of the item is reported only
    // for a generic failure: when the item does not exist on the remote side, there is nothing to download. The
    // identity of the item is reported as well, so that the failure window can trigger its download into the download
    // folder of the user.
    OpenFileJob::Request request;
    const bool urlParsed = OpenFileJob::parseDeeplinkUrl(_url, request);
    DriveDbId driveDbId{0};
    if (_job) driveDbId = _job->driveDbId();
    _appServer->sendFileOpeningFailure(_jobId, failurePhase, failurePhase == FileOpeningPhase::Failed ? webUrl() : CommString{},
                                       urlParsed ? driveDbId : DriveDbId{0},
                                       urlParsed ? CommonUtility::str2CommString(request.remoteNodeId) : CommString{});

    _appServer->unregisterFileOpeningJob(_jobId);
}

CommString FileOpeningHandler::webUrl() const {
    OpenFileJob::Request request;
    if (!OpenFileJob::parseDeeplinkUrl(_url, request)) return {};

    // The kDrive web application opens an item within its parent folder: resolve the parent node id of the item from
    // the file information API. Best effort: when the parent node id cannot be resolved, the failure window shows no
    // download button.
    const auto job = _job;
    if (!job || job->driveDbId() == 0) return {};

    try {
        const auto fileInfoJob = std::make_shared<GetFileInfoJob>(job->driveDbId(), request.remoteNodeId);
        if (const auto exitInfo = fileInfoJob->runSynchronously(); !exitInfo) {
            LOGW_WARN(Log::instance()->getLogger(), L"Error in GetFileInfoJob::runSynchronously - remoteNodeId="
                                                            << CommonUtility::s2ws(request.remoteNodeId) << L": " << exitInfo);
            return {};
        }

        const auto parentNodeId = fileInfoJob->parentNodeId();
        if (parentNodeId.empty()) {
            LOGW_WARN(Log::instance()->getLogger(), L"The file information carries no parent node id - remoteNodeId="
                                                            << CommonUtility::s2ws(request.remoteNodeId));
            return {};
        }

        return CommonUtility::str2CommString(UrlHelper::kDriveWebAppFolderUrl(request.driveId, parentNodeId));
    } catch (const JobException &e) {
        LOGW_WARN(Log::instance()->getLogger(),
                  L"Error in GetFileInfoJob::GetFileInfoJob: error=" << CommonUtility::s2ws(e.what()));
        return {};
    }
}

void FileOpeningHandler::handleFileOpeningCanceled() {
    LOG_INFO(Log::instance()->getLogger(), "File opening canceled on user request - url=" << _url);
    _appServer->unregisterFileOpeningJob(_jobId);
}

ExitInfo FileOpeningHandler::cancel() {
    LOG_INFO(Log::instance()->getLogger(), "Canceling the file opening job - url=" << _url);
    if (_job) {
        _job->cancel();

        return ExitCode::Ok;
    }

    return {ExitCode::DataError, ExitCause::NotFound};
}

void FileOpeningHandler::onFileOpeningStatus(const FileOpeningPhase phase, const int8_t percent, const CommString &itemName,
                                             const CommString &itemPath, const int64_t receivedBytes,
                                             const int64_t totalBytes) const {
    _appServer->sendFileOpeningStatus(_jobId, phase, percent, itemName, itemPath, receivedBytes, totalBytes);
}

bool FileOpeningHandler::isFileOpeningDeeplink(const std::string &urlStr) {
    return OpenFileJob::isFileOpeningUri(urlStr);
}

bool FileOpeningHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> executableExtensions = {
            ".exe", ".com",  ".bat", ".cmd", // Poco's list
            ".ps1", ".psm1", ".msi", ".msix", ".scr", ".sh", // scripts, installers, screensavers
            ".vbs", ".vbe",  ".js",  ".jse",  ".wsf", ".wsh", ".hta", ".pif", ".lnk", ".jar", ".app", ".apk"};

    ItemType itemType;
    const bool isFile = IoHelper::getItemType(path, itemType) && itemType.ioError == IoError::Success &&
                        itemType.nodeType == NodeType::File &&
                        (itemType.linkType == LinkType::None || itemType.targetType != NodeType::Directory);
    return (isFile && Poco::File(Path2Str(path)).canExecute()) ||
           executableExtensions.contains(CommonUtility::toLower(path.extension().string()));
}

Count FileOpeningHandler::maxConcurrentFileOpeningJobs(const Count jobPoolCapacity, const Count reservedWorkerCount) {
    if (jobPoolCapacity <= reservedWorkerCount) {
        return 1; // Degenerate pool: still allow a single flow.
    }

    return std::max<Count>(1, (jobPoolCapacity - reservedWorkerCount) / 2);
}

Count FileOpeningHandler::maxConcurrentFileOpeningJobs() {
    const auto jobManager = SyncJobManagerSingleton::instance();
    return maxConcurrentFileOpeningJobs(jobManager->capacity(), jobManager->highPriorityCapacity());
}

} // namespace KDC
