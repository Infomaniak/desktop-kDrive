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
#include "app/services/syncerrorservice.h"
#include "app/services/syncservice.h"
#include "app/settings/settingswindowcontroller.h"
#include "app/syncconfiguration/localpaths.h"
#include "libcommon/utility/types.h"

#include <QDesktopServices>
#include <QLoggingCategory>
#include <QStorageInfo>
#include <QUrl>

namespace KDC {

namespace {

Q_LOGGING_CATEGORY(lcSyncErrorActionHandler, "gui.v4.syncerroractions", QtInfoMsg)

void openUrl(const QUrl &url) {
    if (!QDesktopServices::openUrl(url)) {
        qCWarning(lcSyncErrorActionHandler) << "Cannot open URL | url:" << url;
    }
}

// Opens a folder of the synchronization in the file manager. `relativePath` is relative to the root; the folder must
// still exist and stay below the root.
void openLocalFolder(const SyncContext &context, const SyncPath &relativePath) {
    const auto folder = resolveExistingPathBelowSyncRoot(context.syncInfo.localPath(), relativePath);
    if (!folder.has_value()) {
        qCWarning(lcSyncErrorActionHandler) << "Local folder unavailable | syncDbId:" << context.syncInfo.dbId();
        return;
    }
    openUrl(QUrl::fromLocalFile(Path2QStr(folder.value())));
}

SyncPath parentFolder(const SyncPath &path) {
    return path.relative_path().lexically_normal().parent_path();
}

// Opens the item on kDrive web when the server knows it, otherwise its local parent folder: the user renames it there.
void openItem(const Error &error, const SyncContext &context) {
    if (!error.remoteNodeId().empty()) {
        openUrl(AppConstants::WebDrive::itemRedirectUri(context.drive.driveId(), error.remoteNodeId()));
        return;
    }
    openLocalFolder(context, parentFolder(error.path()));
}

} // namespace

SyncErrorActionHandler::SyncErrorActionHandler(SyncService &syncService, SyncErrorService &syncErrorService,
                                               SettingsWindowController &settingsWindowController, QObject *const parent) :
    QObject(parent),
    _syncService(syncService),
    _syncErrorService(syncErrorService),
    _settingsWindowController(settingsWindowController) {}

void SyncErrorActionHandler::trigger(const ClassifiedSyncError &classified, const SyncContext &context) {
    const auto &error = classified.error;
    const auto action = describeSyncError(classified.kind, context.drive.admin()).action;
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
            // No disk usage tool is common to every desktop environment: open the volume of the synchronization (D9).
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
        case SyncErrorAction::OpenExclusions:
            _settingsWindowController.requestOpenFileExclusions();
            break;
        case SyncErrorAction::RestartSync:
            _syncService.startSync(syncDbId);
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
        case SyncErrorAction::Explain:
        case SyncErrorAction::None:
        case SyncErrorAction::EnumEnd:
            // The rows of these actions have no button yet.
            qCWarning(lcSyncErrorActionHandler)
                    << "Sync error action not available | action:" << action << "| errorDbId:" << error.dbId();
            break;
    }
}

} // namespace KDC
