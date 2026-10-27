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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "app/mainwindow/syncerroractionhandler.h"

#include "app/appconstants.h"
#include "app/cache/syncerrorsprojection.h"
#include "app/errors/syncerrorcatalog.h"
#include "app/navigation/approuter.h"
#include "app/services/serviceactiontracker.h"
#include "app/services/syncerrorservice.h"
#include "app/services/syncservice.h"
#include "app/settings/settingswindowcontroller.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"
#include "libcommon/utility/utility.h"

#include <QDesktopServices>
#include <QFile>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

#include <filesystem>
#include <optional>
#include <system_error>

namespace KDC {

namespace {

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcSyncErrorActionHandler, "gui.v4.syncerroractions", QtInfoMsg)

constexpr char serviceKeyWatchLimit[] = "inotifyWatchLimit";
constexpr char actionRaiseWatchLimit[] = "raiseWatchLimit";

void openUrl(const QUrl &url) {
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcSyncErrorActionHandler) << "Cannot open URL | url:" << url;
    }
}

// Opens a folder of the synchronization in the file manager. `path` is relative to the root, or absolute (`FileRescuer`
// stores an absolute destination); the folder must still exist and stay below the root.
void openLocalFolder(const SyncContext &context, const SyncPath &path) {
    const auto folder = resolveExistingPathBelowSyncRoot(context.syncInfo.localPath(), path);
    if (!folder.has_value()) {
        qCWarning(lcSyncErrorActionHandler) << "Local folder unavailable | syncDbId:" << context.syncInfo.dbId();
        return;
    }
    openUrl(QUrl::fromLocalFile(Path2QStr(folder.value())));
}

SyncPath parentFolder(const SyncPath &path) {
    return path.lexically_normal().parent_path();
}

// Opens the item on kDrive web when the server knows it, otherwise its local parent folder: the user renames it there.
void openItem(const Error &error, const SyncContext &context) {
    if (!error.remoteNodeId().empty()) {
        openUrl(AppConstants::WebDrive::itemRedirectUri(context.drive.driveId(), error.remoteNodeId()));
        return;
    }
    openLocalFolder(context, parentFolder(error.path()));
}

// Opens the folder that contains the synchronization root, which may itself be gone (moved, deleted or on an unplugged
// disk). It is above the root, so it is not resolved below it.
void openSyncParentFolder(const SyncContext &context) {
    auto syncRoot = context.syncInfo.localPath().lexically_normal();
    if (!syncRoot.has_filename()) {
        syncRoot = syncRoot.parent_path(); // Trailing separator
    }
    const auto folder = syncRoot.parent_path();
    if (std::error_code errorCode; folder.empty() || !std::filesystem::is_directory(folder, errorCode)) {
        qCWarning(lcSyncErrorActionHandler) << "Synchronization parent folder unavailable | syncDbId:" << context.syncInfo.dbId();
        return;
    }
    openUrl(QUrl::fromLocalFile(Path2QStr(folder)));
}


// Run as root through `pkexec`, so it trusts nothing: one numeric argument, a fixed file. Exit codes 126 and 127 are
// taken by pkexec itself.
constexpr char watchLimitScript[] = R"KDRIVE(#!/bin/sh
# Written by kDrive before each use, do not edit.
# Raises the inotify watches limit: kDrive watches every folder of a synchronization.
# Exit codes: 2 invalid limit, 3 write failed, 4 sysctl failed, 5 limit not applied, 6 not run as root by pkexec.
set -u

# Only through pkexec, as root.
[ -n "${PKEXEC_UID:-}" ] && [ "$(id -u)" -eq 0 ] || exit 6

# A single number, between the minimum default of the kernel and the int maximum.
[ "$#" -eq 1 ] || exit 2
limit=$1
case "$limit" in
    '' | *[!0-9]*) exit 2 ;;
esac
[ "${#limit}" -le 10 ] || exit 2
[ "$limit" -ge 8192 ] && [ "$limit" -le 2147483647 ] || exit 2

conf=/etc/sysctl.d/90-kdrive.conf
mkdir -p /etc/sysctl.d || exit 3
tmp=$(mktemp /etc/sysctl.d/.90-kdrive.conf.XXXXXX) || exit 3
trap 'rm -f "$tmp"' EXIT
printf '# Written by kDrive: raises the inotify watches limit.\nfs.inotify.max_user_watches=%s\n' "$limit" > "$tmp" || exit 3
chmod 644 "$tmp" || exit 3
mv -f "$tmp" "$conf" || exit 3

sysctl -p "$conf" > /dev/null || exit 4
[ "$(cat /proc/sys/fs/inotify/max_user_watches)" = "$limit" ] || exit 5
)KDRIVE";

// Exit code of pkexec when the user dismisses the authentication dialog.
constexpr uint8_t pkexecCancelledExitCode = 126;

// Writes the script to the state directory of the app, executable by its owner only, then reads it back. pkexec runs a
// file, so that polkit shows its path rather than a shell command line; a file inside the AppImage is not readable by
// root.
std::optional<QString> writeWatchLimitScript() {
    SyncPath directory;
    if (const auto exitInfo = CommonUtility::stateDirectoryPath(directory); !exitInfo) {
        qCWarning(lcSyncErrorActionHandler) << "State directory unavailable | code:" << exitInfo.code()
                                            << "| cause:" << exitInfo.cause();
        return std::nullopt;
    }
    if (std::error_code errorCode; !std::filesystem::create_directories(directory, errorCode) && errorCode) {
        qCWarning(lcSyncErrorActionHandler) << "Cannot create the state directory | error:" << errorCode.message();
        return std::nullopt;
    }
    const SyncPath scriptPath = directory / "update_inotify_limit";
    const auto path = Path2QStr(scriptPath);
    const QByteArray content(watchLimitScript);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit()) {
        qCWarning(lcSyncErrorActionHandler) << "Cannot write the inotify watches script | path:" << path
                                            << "| error:" << file.errorString();
        return std::nullopt;
    }
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        qCWarning(lcSyncErrorActionHandler) << "Cannot make the inotify watches script executable | path:" << path;
        return std::nullopt;
    }
    if (QFile written(path); !written.open(QIODevice::ReadOnly) || written.readAll() != content) {
        qCWarning(lcSyncErrorActionHandler) << "Inotify watches script changed after writing | path:" << path;
        return std::nullopt;
    }
    return path;
}

} // namespace

SyncErrorActionHandler::SyncErrorActionHandler(SyncService &syncService, SyncErrorService &syncErrorService,
                                               SettingsWindowController &settingsWindowController,
                                               ServiceActionTracker &serviceActionTracker, QObject *const parent) :
    QObject(parent),
    _syncService(syncService),
    _syncErrorService(syncErrorService),
    _settingsWindowController(settingsWindowController),
    _serviceActionTracker(serviceActionTracker) {
    (void) connect(&_serviceActionTracker, &ServiceActionTracker::actionPendingChanged, this,
                   [this](const ServiceActionTracker::ServiceKey &serviceKey, const ServiceActionTracker::ActionKey &actionKey,
                          const ServiceActionTracker::ScopeId scopeId, const bool) {
                       if (serviceKey == serviceKeyWatchLimit && actionKey == actionRaiseWatchLimit) {
                           emit watchLimitRaisePendingChanged(scopeId);
                       }
                   });
}

void SyncErrorActionHandler::trigger(const ClassifiedSyncError &classified, const SyncContext &context) {
    run(describeSyncError(classified.kind, context.drive.admin()).action, classified, context);
}

void SyncErrorActionHandler::run(const SyncErrorAction action, const ClassifiedSyncError &classified,
                                 const SyncContext &context) {
    const auto &error = classified.error;
    const auto syncDbId = context.syncInfo.dbId();
    qCInfo(lcSyncErrorActionHandler) << "Sync error action requested | kind:" << classified.kind << "| action:" << action
                                     << "| errorDbId:" << error.dbId() << "| syncDbId:" << syncDbId;

    switch (action) {
        case SyncErrorAction::Rename:
            openItem(error, context);
            break;
        case SyncErrorAction::OpenParentFolder:
            openLocalFolder(context, parentFolder(error.path()));
            break;
        case SyncErrorAction::OpenRescueFolder:
            openLocalFolder(context, parentFolder(error.destinationPath()));
            break;
        case SyncErrorAction::ManageDiskSpace:
            // No disk usage tool is common to every desktop environment: open the volume of the synchronization.
            openUrl(QUrl::fromLocalFile(QStorageInfo(Path2QStr(context.syncInfo.localPath())).rootPath()));
            break;
        case SyncErrorAction::ManageStorage:
        case SyncErrorAction::RenewSubscription:
            openUrl(AppConstants::Drive::renewalUri(context.drive));
            break;
        case SyncErrorAction::WakeUp:
            // Opening the drive on the web wakes it up.
            openUrl(AppConstants::WebDrive::destinationUri(context.drive.driveId(),
                                                           AppConstants::WebDrive::Destination::OnlineDrive));
            break;
        case SyncErrorAction::OpenSyncParentFolder:
            openSyncParentFolder(context);
            break;
        case SyncErrorAction::OpenExclusions:
            _settingsWindowController.requestOpenFileExclusions();
            break;
        case SyncErrorAction::CreateNewSync:
            _settingsWindowController.requestOpenAccounts();
            break;
        case SyncErrorAction::Explain:
            emit explanationRequested(error.dbId());
            break;
        case SyncErrorAction::RestartSync:
            _syncService.startSync(syncDbId);
            break;
        case SyncErrorAction::RaiseWatchLimit:
            raiseWatchLimit(classified, context);
            break;
        case SyncErrorAction::RefreshErrors:
            _syncErrorService.refreshErrors(syncDbId);
            break;
        case SyncErrorAction::Reconnect:
            emit loginRequested();
            break;
        case SyncErrorAction::ContactSupport:
            AppRouter::openSupport();
            break;
        case SyncErrorAction::ChooseVersion:
        case SyncErrorAction::None:
        case SyncErrorAction::EnumEnd:
            // The rows of these actions have no button yet.
            qCWarning(lcSyncErrorActionHandler)
                    << "Sync error action not available | action:" << action << "| errorDbId:" << error.dbId();
            break;
    }
}

bool SyncErrorActionHandler::isWatchLimitRaisePending(const qint64 syncDbId) const {
    return _serviceActionTracker.isActionPending(serviceKeyWatchLimit, actionRaiseWatchLimit, syncDbId);
}

// Raises the limit with the script, as root through pkexec, then restarts the synchronization. Without pkexec or when the
// raise fails, the explanation dialog gives the command to run in a terminal.
void SyncErrorActionHandler::raiseWatchLimit(const ClassifiedSyncError &classified, const SyncContext &context) {
    const auto errorDbId = classified.error.dbId();
    const qint64 syncDbId = context.syncInfo.dbId();
    // The limit is the same for every synchronization: one raise at a time.
    if (_serviceActionTracker.isServicePending(serviceKeyWatchLimit)) {
        qCInfo(lcSyncErrorActionHandler) << "Inotify watches limit raise already running | syncDbId:" << syncDbId;
        return;
    }
    const auto pkexecPath = QStandardPaths::findExecutable(u"pkexec"_s);
    if (pkexecPath.isEmpty()) {
        qCInfo(lcSyncErrorActionHandler) << "pkexec not found, showing the inotify watches command";
        emit explanationRequested(errorDbId);
        return;
    }
    const auto scriptPath = writeWatchLimitScript();
    if (!scriptPath.has_value()) {
        emit explanationRequested(errorDbId, WatchLimitRaiseFailure::Failed);
        return;
    }

    const auto limit = proposedWatchLimit(readWatchLimit());
    auto *const process = new QProcess(this);
    // The script prints nothing on success; pkexec and the commands it runs explain their failures on stderr, logged
    // with the exit code.
    process->setProcessChannelMode(QProcess::MergedChannels);
    (void) connect(process, &QProcess::finished, this,
                   [this, process, errorDbId, syncDbId](const int exitCode, const QProcess::ExitStatus exitStatus) {
                       process->deleteLater();
                       const bool raised = exitStatus == QProcess::NormalExit && exitCode == 0;
                       qCInfo(lcSyncErrorActionHandler)
                               << "Inotify watches limit raise finished | exitCode:" << exitCode << "| exitStatus:" << exitStatus
                               << "| limit:" << readWatchLimit() << "| output:" << process->readAll().trimmed();
                       if (raised) {
                           finishWatchLimitRaise(errorDbId, syncDbId, WatchLimitRaiseFailure::None);
                           _syncService.startSync(syncDbId);
                           return;
                       }
                       const bool cancelled = exitStatus == QProcess::NormalExit && exitCode == pkexecCancelledExitCode;
                       finishWatchLimitRaise(errorDbId, syncDbId,
                                             cancelled ? WatchLimitRaiseFailure::Cancelled : WatchLimitRaiseFailure::Failed);
                   });
    (void) connect(process, &QProcess::errorOccurred, this,
                   [this, process, errorDbId, syncDbId](const QProcess::ProcessError error) {
                       // The other errors end with `finished`.
                       if (error != QProcess::FailedToStart) {
                           return;
                       }
                       qCWarning(lcSyncErrorActionHandler) << "pkexec failed to start | error:" << process->errorString();
                       process->deleteLater();
                       finishWatchLimitRaise(errorDbId, syncDbId, WatchLimitRaiseFailure::Failed);
                   });

    _serviceActionTracker.beginAction(serviceKeyWatchLimit, actionRaiseWatchLimit, syncDbId);
    qCInfo(lcSyncErrorActionHandler) << "Raising the inotify watches limit | script:" << *scriptPath << "| limit:" << limit
                                     << "| syncDbId:" << syncDbId;
    process->start(pkexecPath, {*scriptPath, QString::number(limit)});
}

void SyncErrorActionHandler::finishWatchLimitRaise(const ErrorDbId errorDbId, const qint64 syncDbId,
                                                   const WatchLimitRaiseFailure raiseFailure) {
    _serviceActionTracker.endAction(serviceKeyWatchLimit, actionRaiseWatchLimit, syncDbId);
    if (raiseFailure != WatchLimitRaiseFailure::None) {
        emit explanationRequested(errorDbId, raiseFailure);
    }
}

} // namespace KDC
