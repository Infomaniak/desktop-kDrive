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

#include "filemanager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QPointer>
#include <QUrl>

namespace KDC {

using namespace Qt::StringLiterals;

namespace {
Q_LOGGING_CATEGORY(lcFileManager, "gui.v4.filemanager", QtInfoMsg)

constexpr auto fileManagerService = "org.freedesktop.FileManager1"_L1;
constexpr auto fileManagerPath = "/org/freedesktop/FileManager1"_L1;
constexpr auto fileManagerInterface = "org.freedesktop.FileManager1"_L1;

// Without a file manager service, every call fails the same way: log it once.
bool serviceUnavailableLogged = false;

bool openContainingFolder(const SyncPath &path) {
    const SyncPath folder = path.parent_path();
    if (folder.empty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(Path2QStr(folder)))) {
        qCWarning(lcFileManager) << "Desktop service failed to open the containing folder | path:" << Path2QStr(path);
        return false;
    }
    return true;
}
} // namespace

void revealInFileManager(const SyncPath &path, const QObject *const context, std::function<void()> onFailure) {
    auto message = QDBusMessage::createMethodCall(fileManagerService, fileManagerPath, fileManagerInterface, u"ShowItems"_s);
    // Without a startup id, the file manager may open behind the application window on Wayland.
    message << QStringList{QUrl::fromLocalFile(Path2QStr(path)).toString(QUrl::FullyEncoded)} << QString();

    // The service may only start on this call: it is not looked up beforehand.
    auto *const watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message));
    (void) QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                            [path, guard = QPointer(context), hasContext = context != nullptr,
                             onFailure = std::move(onFailure)](QDBusPendingCallWatcher *const finishedWatcher) {
                                finishedWatcher->deleteLater();
                                const QDBusPendingReply<> reply = *finishedWatcher;
                                if (!reply.isError()) {
                                    return;
                                }

                                if (!serviceUnavailableLogged) {
                                    serviceUnavailableLogged = true;
                                    qCWarning(lcFileManager) << "File manager service unavailable, opening the folder instead"
                                                             << "| error:" << reply.error().name() << reply.error().message();
                                }
                                if (openContainingFolder(path) || !onFailure) {
                                    return;
                                }

                                const bool contextDestroyed = hasContext && !guard;
                                if (!contextDestroyed) {
                                    onFailure();
                                }
                            });
}

} // namespace KDC
