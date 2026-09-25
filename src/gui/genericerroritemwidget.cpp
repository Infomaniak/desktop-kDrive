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

#include "genericerroritemwidget.h"
#include "clientgui.h"
#include "custommessagebox.h"
#include "parameterscache.h"

#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>

namespace KDC {

#define GENERICERRORITEMWIDGET_NEW_ERROR_MSG "Failed to create GenericErrorItemWidget instance!"

static const QString dateFormat = "d MMM yyyy - HH:mm";

GenericErrorItemWidget::GenericErrorItemWidget(std::shared_ptr<ClientGui> gui, const QString &errorMsg, const Error &error,
                                               QWidget *parent) :
    AbstractFileItemWidget(parent),
    _gui(gui),
    _error(error),
    _errorMsg(errorMsg) {
    init();
}

void GenericErrorItemWidget::init() {
    setMessage(_errorMsg);

    // Path layout
    if (_error.level() == ErrorLevel::SyncPal || _error.level() == ErrorLevel::Node) {
        const auto &syncInfoMapIt = _gui->syncInfoMap().find(_error.syncDbId());
        if (syncInfoMapIt == _gui->syncInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt == _gui->driveInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        // Path
        if (_error.level() == ErrorLevel::SyncPal) {
            setDriveName(QString::fromStdString(driveInfoMapIt->second.name()), Path2QStr(syncInfoMapIt->second.localPath()));
            setPathIconColor(QColor(QString::fromStdString(driveInfoMapIt->second.color())));
        } else if (_error.level() == ErrorLevel::Node) {
            const bool useDestPath = _error.cancelType() == CancelType::MoveToBinFailed ||
                                     _error.cancelType() == CancelType::FileRescued ||
                                     _error.conflictType() == ConflictType::EditDelete;
            const QString &filePath = useDestPath ? Path2QStr(_error.destinationPath()) : Path2QStr(_error.path());
            setPathAndName(filePath, _error.nodeType());
        }
    }

    // Right layout
    auto fileDateLabel = new QLabel(this);
    fileDateLabel->setObjectName("fileDateLabel");
    const auto errorTime = _error.time();
    const QDateTime dateTime = errorTime ? QDateTime::fromSecsSinceEpoch(errorTime)
                                         : QDateTime::currentDateTime(); // If error time is not set, use current time.
    fileDateLabel->setText(GuiUtility::getDateForCurrentLanguage(dateTime, dateFormat));

    addCustomWidget(fileDateLabel);
}

void GenericErrorItemWidget::openFolder(const QString &path) {
    const auto syncInfoMapIt = _gui->syncInfoMap().find(_error.syncDbId());
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder path %1.").arg(path), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    if (openInWebview()) {
        // Open in webview instead
        const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt != _gui->driveInfoMap().end()) {
            _gui->onOpenWebviewItem(syncInfoMapIt->second.driveDbId(), QString::fromStdString(_error.remoteNodeId()));
            return;
        }
    }
    // Open on local filesystem (open the parent folder for an item of file type).
    const auto absolutePath =
            SyncPath(path.toStdString()).is_absolute() ? path : (Path2QStr(syncInfoMapIt->second.localPath()) + "/" + path);
    const auto folderPath = GuiUtility::getFolderPath(absolutePath, _error.nodeType());
    AbstractFileItemWidget::openFolder(folderPath);
}

bool GenericErrorItemWidget::openInWebview() const {
    return _error.inconsistencyType() == InconsistencyType::PathLength || _error.inconsistencyType() == InconsistencyType::Case ||
           _error.inconsistencyType() == InconsistencyType::ForbiddenChar ||
           _error.inconsistencyType() == InconsistencyType::ForbiddenCharEndWithSpace ||
           _error.inconsistencyType() == InconsistencyType::ReservedName ||
           _error.inconsistencyType() == InconsistencyType::NameLength ||
           _error.inconsistencyType() == InconsistencyType::NotYetSupportedChar ||
           (_error.conflictType() == ConflictType::EditDelete && !_error.remoteNodeId().empty()) ||
           (_error.exitCode() == ExitCode::BackError && _error.exitCause() == ExitCause::NotFound);
}

} // namespace KDC
