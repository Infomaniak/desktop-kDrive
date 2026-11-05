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
#include "libcommon/utility/utility.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/utility/utility.h"
#include "libsyncengine/jobs/syncjobmanager.h"
#include "libsyncengine/syncpal/syncpal.h"

#include <QDesktopServices>

#include <Poco/File.h>

#include <log4cplus/loggingmacros.h>

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

FileOpeningHandler::FileOpeningHandler(AppServer *const appServer, std::string url, QObject *const parent) :
    QObject(parent),
    _appServer(appServer),
    _url(std::move(url)) {}

void FileOpeningHandler::queueOpenFileJob() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url);

    if (OpenFileJob::Request request; !OpenFileJob::parseDeeplinkUrl(_url, request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url);
        handleFileOpeningFailure(tr("This kDrive link is invalid."));

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
    _appServer->registerFileOpeningJob(_jobId, this);
    job->setFeedbackCallback([this](const FileOpeningPhase phase, const int8_t percent, const CommString &itemName) {
        // Job manager worker thread. Hand the state over to the application thread. `this` is used as the context
        // object: if the handler has been destroyed in the meantime, the call is simply discarded.
        (void) QMetaObject::invokeMethod(
                this, [this, phase, percent, itemName] { onFileOpeningStatus(phase, percent, itemName); }, Qt::QueuedConnection);
    });
    std::weak_ptr<OpenFileJob> weakJob = job;
    job->setAdditionalCallback([this, weakJob](UniqueId) {
        const auto job = weakJob.lock();
        if (!job) return;

        // Job manager worker thread. Hand the outcome over to the application thread. `this` is used as the context
        // object: if the handler has been destroyed in the meantime, the call is simply discarded.
        (void) QMetaObject::invokeMethod(
                this,
                [this, outcome = job->outcome(), localPath = job->localPath(), syncDbId = job->syncDbId()] {
                    onOpenFileJobFinished(outcome, localPath, syncDbId);
                },
                Qt::QueuedConnection);
    });
    SyncJobManagerSingleton::instance()->queueAsyncJob(job, Poco::Thread::PRIO_NORMAL);
}

void FileOpeningHandler::onOpenFileJobFinished(const OpenFileJob::Outcome outcome, const SyncPath &localPath,
                                               const SyncDbId syncDbId) {
    // Close the pop-up window on the client side, if one is open (item being synchronized or download in progress).
    // The item name is irrelevant here: the clients close the window upon receiving the Finished phase.
    _appServer->sendFileOpeningStatus(_jobId, FileOpeningPhase::Finished, -1, {});

    switch (outcome) {
        case OpenFileJob::Outcome::ReadyToOpen:
            (void) openLocalItem(localPath, syncDbId);
            break;
        case OpenFileJob::Outcome::NotFound:
            handleFileOpeningFailure(tr("The element requested from this kDrive link could not be found."));
            break;
        case OpenFileJob::Outcome::ResolveTimedOut:
            handleFileOpeningFailure(
                    tr("The element requested from this kDrive link could not be synchronized. Please try again later."));
            break;
        case OpenFileJob::Outcome::HydrationTimedOut:
            handleFileOpeningFailure(tr("The download of the element requested from this kDrive link took too long."));
            break;
        case OpenFileJob::Outcome::Failed:
            handleFileOpeningFailure(tr("The element requested from this kDrive deeplink could not be opened."));
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

        handleFileOpeningFailure(tr("The download of the element requested from this kDrive link failed."));

        return ExitCode::SystemError;
    }

    // Do not attempt to open a dehydrated placeholder if the engine of the sync is not running: the system is unable
    // to hydrate it.
    if (isDeHydrated && !isSyncEngineRunning(syncDbId)) {
        handleFileOpeningFailure(
                tr("The download of the element requested from this kDrive link failed because its sync is not running."));

        return ExitCode::DataError;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        handleFileOpeningFailure(tr("The element requested from this kDrive link could not be opened."));

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

void FileOpeningHandler::notifyUser(const QString &message) const {
    _appServer->sendShowNotification(QString::fromStdString(Theme::instance()->appName()), message);
}

void FileOpeningHandler::handleFileOpeningSuccess() {
    emit finished(true);
}

void FileOpeningHandler::handleFileOpeningFailure(const QString &userMessage) {
    LOG_WARN(Log::instance()->getLogger(), "Failed to open item from deeplink URL - url=" << _url);
    notifyUser(userMessage);
    emit finished(false);
}

void FileOpeningHandler::handleFileOpeningCanceled() {
    LOG_INFO(Log::instance()->getLogger(), "File opening canceled on user request - url=" << _url);
    emit finished(false);
}

ExitInfo FileOpeningHandler::cancel() {
    LOG_INFO(Log::instance()->getLogger(), "Canceling the file opening job - url=" << _url);
    if (const auto job = _job.lock()) {
        job->cancel();

        return ExitCode::Ok;
    }

    return {ExitCode::DataError, ExitCause::NotFound};
}

void FileOpeningHandler::onFileOpeningStatus(const FileOpeningPhase phase, const int8_t percent,
                                             const CommString &itemName) const {
    _appServer->sendFileOpeningStatus(_jobId, phase, percent, itemName);
}

bool FileOpeningHandler::isFileOpeningDeeplink(const std::string &urlStr) {
    return OpenFileJob::isFileOpeningUri(urlStr);
}

bool FileOpeningHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> executableExtensions = {
            ".exe", ".com",  ".bat", ".cmd", // Poco's list
            ".ps1", ".psm1", ".msi", ".msix", ".scr", ".sh", // scripts, installers, screensavers
            ".vbs", ".vbe",  ".js",  ".jse",  ".wsf", ".wsh", ".hta", ".pif", ".lnk", ".jar", ".app", ".apk"};

    return Poco::File(Path2Str(path)).canExecute() ||
           executableExtensions.contains(CommonUtility::toLower(path.extension().string()));
}

} // namespace KDC
