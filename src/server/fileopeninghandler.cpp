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

#include <Poco/Exception.h>
#include <Poco/URI.h>

#include <log4cplus/loggingmacros.h>

#include <charconv>
#include <unordered_set>
#include <vector>

namespace KDC {

namespace {
//! Parses `urlStr` into `uri`. Returns false if `urlStr` is not a syntactically valid URI.
bool parseUri(const std::string &urlStr, Poco::URI &uri) {
    try {
        uri = Poco::URI(urlStr);

        return true;
    } catch (const Poco::URISyntaxException &) {
        return false;
    }
}

//! Returns true if `uri` is a file opening URI, i.e. "kdrive://open/...". Poco::URI stores the scheme and the host
//! in lowercase, making the comparisons case-insensitive.
bool isOpenFileUri(const Poco::URI &uri) {
    return uri.getScheme() == "kdrive" && uri.getHost() == "open";
}

//! Parses a strictly positive integer identifier (e.g. a user id or a drive id) into `id`. Returns false if `idStr`
//! does not exactly represent one.
bool parsePositiveId(std::string_view idStr, int64_t &id) {
    // std::from_chars parses the whole string strictly (no whitespace, no leading '+', no trailing garbage) and never
    // throws.
    const auto result = std::from_chars(idStr.data(), idStr.data() + idStr.size(), id);

    return result.ec == std::errc{} && result.ptr == idStr.data() + idStr.size() && id > 0;
}
} // namespace

FileOpeningHandler::FileOpeningHandler(AppServer *appServer, std::string url, QObject *parent) :
    QObject(parent),
    _appServer(appServer),
    _url(std::move(url)) {}

void FileOpeningHandler::queueOpenFileJob() {
    LOG_INFO(Log::instance()->getLogger(), "Processing file opening URL - url=" << _url);

    Request request;
    if (!parseDeeplinkUrl(_url, request)) {
        LOG_WARN(Log::instance()->getLogger(), "Invalid file opening URL - url=" << _url);
        handleFileOpeningFailure("This kDrive link is invalid.");

        return;
    }

    auto getSyncPal = [this](SyncDbId syncDbId) -> std::shared_ptr<SyncPal> {
        const std::scoped_lock lock(_appServer->syncPalMapMutex);
        const auto syncPalMapIt = _appServer->syncPalMap.find(syncDbId);

        return syncPalMapIt == _appServer->syncPalMap.end() ? nullptr : syncPalMapIt->second;
    };

    auto getVfs = [this](SyncDbId syncDbId) -> std::shared_ptr<Vfs> {
        std::shared_ptr<Vfs> vfs;
        (void) _appServer->getVfs(syncDbId, vfs);

        return vfs;
    };

    const auto job = std::make_shared<OpenFileJob>(_url, std::move(getSyncPal), std::move(getVfs));
    job->setAdditionalCallback([this, job](UniqueId) {
        // Job manager worker thread. Hand the outcome over to the application thread. `this` is used as the context
        // object: if the handler has been destroyed in the meantime, the call is simply discarded.
        QMetaObject::invokeMethod(
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
    switch (outcome) {
        case OpenFileJob::Outcome::ReadyToOpen:
            (void) openLocalItem(localPath, syncDbId);
            break;
        case OpenFileJob::Outcome::NotFound:
            handleFileOpeningFailure("The element requested from this kDrive link could not be found.");
            break;
        case OpenFileJob::Outcome::ResolveTimedOut:
            handleFileOpeningFailure(
                    "The element requested from this kDrive link could not be synchronized. Please try again later.");
            break;
        case OpenFileJob::Outcome::HydrationTimedOut:
            handleFileOpeningFailure("The download of the element requested from this kDrive link took too long.");
            break;
        case OpenFileJob::Outcome::Failed:
            handleFileOpeningFailure("The element requested from this kDrive deeplink could not be opened.");
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

        handleFileOpeningFailure("The download of the element requested from this kDrive link failed.");

        return ExitCode::SystemError;
    }

    // Do not attempt to open a dehydrated placeholder if the engine of the sync is not running: the system is unable
    // to hydrate it.
    if (isDeHydrated && !isSyncEngineRunning(syncDbId)) {
        handleFileOpeningFailure(
                "The download of the element requested from this kDrive link failed because its sync is not running.");

        return ExitCode::DataError;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(pathToOpen)))) {
        LOGW_WARN(Log::instance()->getLogger(), L"Error in QDesktopServices::openUrl - " << Utility::formatSyncPath(pathToOpen));
        handleFileOpeningFailure("The element requested from this kDrive link could not be opened.");

        return ExitCode::SystemError;
    }

    LOGW_INFO(Log::instance()->getLogger(), L"Item opened from URL - " << Utility::formatSyncPath(pathToOpen));
    handleFileOpeningSuccess();

    return ExitCode::Ok;
}

bool FileOpeningHandler::isSyncEngineRunning(const SyncDbId syncDbId) const {
    const std::scoped_lock lock(_appServer->syncPalMapMutex);
    return _appServer->syncPalMap.contains(syncDbId);
}

void FileOpeningHandler::notifyUser(const std::string &message) const {
    _appServer->sendShowNotification(QString::fromStdString(Theme::instance()->appName()), QString::fromStdString(message));
}

void FileOpeningHandler::handleFileOpeningSuccess() {
    emit finished(true);
}

void FileOpeningHandler::handleFileOpeningFailure(const std::string &userMessage) {
    LOG_WARN(Log::instance()->getLogger(), "Failed to open item from deeplink URL - url=" << _url);
    notifyUser(userMessage);
    emit finished(false);
}

bool FileOpeningHandler::isFileOpeningDeeplink(const std::string &urlStr) {
    Poco::URI uri;
    return parseUri(urlStr, uri) && isOpenFileUri(uri);
}

bool FileOpeningHandler::parseDeeplinkUrl(const std::string &urlStr, Request &request) {
    Poco::URI uri;
    if (!parseUri(urlStr, uri) || !isOpenFileUri(uri)) return false;

    // Poco::URI decodes the path and excludes the query and the fragment while parsing, and getPathSegments drops the
    // empty segments produced by the leading, doubled, and trailing slashes.
    std::vector<std::string> segments;
    uri.getPathSegments(segments);
    if (segments.size() != 6 || segments[0] != "user" || segments[2] != "drive" || segments[4] != "file") return false;

    UserId userId;
    if (!parsePositiveId(segments[1], userId)) return false;
    request.user.setUserId(userId);

    if (!parsePositiveId(segments[3], request.driveId)) return false;
    request.remoteNodeId = segments[5];

    return !request.remoteNodeId.empty();
}

bool FileOpeningHandler::shouldOpenParentFolder(const SyncPath &path) {
    static const std::unordered_set<std::string> executableExtensions = {".exe", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1",
                                                                         ".vbs", ".js",  ".jar", ".sh",  ".app", ".apk"};
    return executableExtensions.contains(CommonUtility::toLower(path.extension().string()));
}

} // namespace KDC
