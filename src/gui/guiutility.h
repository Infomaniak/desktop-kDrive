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

#ifndef GUIUTILITY_H
#define GUIUTILITY_H

#include "info/syncinfoclient.h"
#include "libcommon/utility/types.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QWidget>

class QHelpEvent;
class QLayout;

namespace KDC {

class CustomToolTip;

namespace GuiUtility {
static const QString learnMoreLink = QString("learnMoreLink");
static const QString loginLink = QString("loginLink");

enum systrayPosition {
    Top = 0,
    Bottom,
    Left,
    Right
};

enum WizardAction {
    OpenFolder = 0,
    OpenParameters,
    AddDrive
};

struct StatusInfo {
        bool _unresolvedConflicts = false;
        KDC::SyncStatus _status = KDC::SyncStatus::Undefined;
        qint64 _syncedFiles = 0;
        qint64 _totalFiles = 0;
        qint64 _estimatedRemainingTime = 0;
        KDC::SyncStep _syncStep = KDC::SyncStep::None;
        bool _oneSyncInPropagationStep = false;
        bool _liteSyncActivated = false;
        bool _disconnected = false;
};

/** Open an url in the browser.
 *
 * If launching the browser fails, display a message.
 */
bool openBrowser(const QUrl &url, QWidget *errorWidgetParent);

/** Start composing a new email message.
 *
 * If launching the email program fails, display a message.
 */
bool openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent);

QPixmap getAvatarFromImage(const QImage &image);
QIcon getIconWithColor(const QString &path, const QColor &color = QColor());
QIcon getIconMenuWithColor(const QString &path, const QColor &color = QColor());

systrayPosition getSystrayPosition(const QScreen *const screen);
bool isPointInSystray(QScreen *screen, const QPoint &point);

bool isDarkTheme();
void setStyle(QApplication *app);
void setStyle(QApplication *app, bool isDarkTheme);

QString getFileStatusIconPath(KDC::SyncFileStatus status);
QString getSyncStatusIconPath(StatusInfo &statusInfo);
QString getSyncStatusText(StatusInfo &statusInfo);
QString getDriveStatusIconPath(StatusInfo &statusInfo);
bool getPauseActionAvailable(KDC::SyncStatus status);
bool getResumeActionAvailable(KDC::SyncStatus status);
QColor getShadowColor(bool dialog = false);
QUrl getUrlFromLocalPath(const QString &path);
int getQFontWeightFromQSSFontWeight(int weight);
qint64 folderSize(const QString &dirPath);
qint64 folderDiskSize(const QString &dirPath);

// Returns `path` if `nodeType` is `NodeType::Directory`, else the parent folder path.
QString getFolderPath(const QString &path, NodeType nodeType);

// Opens the folder indicated by `path`, if `path` is valid, otherwise tries to open the parent folder.
// Returns true if `path` is empty, or if the folder indicated by `path` is successfully open,
// or if the parent folder is successfully open. Returns false otherwise.
bool openFolder(const QString &path);

QWidget *getTopLevelWidget(QWidget *widget);
void forceUpdate(QWidget *widget);
void invalidateLayout(QLayout *layout);
bool warnOnInvalidSyncFolder(const QString &dirPath, const std::map<int, SyncInfoClient> &syncInfoMap, QWidget *parent);

void makePrintablePath(QString &path, const uint64_t maxSize = 50);

QLocale languageToQLocale(Language language);
QString getDateForCurrentLanguage(const QDateTime &dateTime, const QString &dateFormat);

/**
 * @brief The number of blacklisted items is limited to 50 to avoid too long URI (HTTP 414 error). Display a popup message if the
 * size is too high.
 * @param blacklistSize The number of selected folders to be blacklisted.
 * @param parent The parent widget for the `QMessageBox`.
 * @return `true` if the number of selected folders to be blacklisted is not too high.
 */
bool checkBlacklistSize(const qsizetype blacklistSize, QWidget *parent);

#ifdef Q_OS_LINUX
bool getLinuxDesktopType(QString &type, QString &version);
#endif

template<class C>
void setEnabledRecursively(C *root, bool enabled) {
    if (!root) return;

    root->setEnabled(enabled);
    for (auto *child: root->template findChildren<QLayout *>()) {
        setEnabledRecursively(child, enabled);
    }
    for (auto *child: root->template findChildren<QWidget *>()) {
        if (!enabled) child->setToolTip("");
        setEnabledRecursively(child, enabled);
    }
}
} // namespace GuiUtility

} // namespace KDC

#endif
