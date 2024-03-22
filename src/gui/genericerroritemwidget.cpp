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

#include "genericerroritemwidget.h"
#include "guiutility.h"
#include "clientgui.h"
#include "libcommon/utility/utility.h"
#include "custommessagebox.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>

#include <iostream>

namespace KDC {

#define GENERICERRORITEMWIDGET_NEW_ERROR_MSG "Failed to create GenericErrorItemWidget instance!"

static const QString dateFormat = "d MMM yyyy - HH:mm";

GenericErrorItemWidget::GenericErrorItemWidget(std::shared_ptr<ClientGui> gui, const QString &errorMsg,
                                               const ErrorInfo &errorInfo, QWidget *parent)
    : AbstractFileItemWidget(parent), _gui(gui), _errorInfo(errorInfo), _errorMsg(errorMsg) {
    init();
}

void GenericErrorItemWidget::init() {
    setMessage(_errorMsg);

    // Path layout
    if (_errorInfo.level() == ErrorLevelSyncPal || _errorInfo.level() == ErrorLevelNode) {
        const auto &syncInfoMapIt = _gui->syncInfoMap().find(_errorInfo.syncDbId());
        if (syncInfoMapIt == _gui->syncInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt == _gui->driveInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        // Path
        QString pathStr;
        if (_errorInfo.level() == ErrorLevelSyncPal) {
            setDriveName(driveInfoMapIt->second.name(), syncInfoMapIt->second.localPath());
            setPathIconColor(driveInfoMapIt->second.color());
        } else if (_errorInfo.level() == ErrorLevelNode) {
            bool useDestPath = _errorInfo.cancelType() == CancelTypeAlreadyExistRemote ||
                               _errorInfo.cancelType() == CancelTypeMoveToBinFailed ||
                               _errorInfo.conflictType() == ConflictTypeEditDelete;
            const QString &filePath = useDestPath ? _errorInfo.destinationPath() : _errorInfo.path();
            setFilePath(filePath, _errorInfo.nodeType());
        }
    }

    // Right layout
    QLabel *fileDateLabel = new QLabel(this);
    fileDateLabel->setObjectName("fileDateLabel");
    fileDateLabel->setText(QDateTime::fromSecsSinceEpoch(_errorInfo.getTime()).toString(dateFormat));
    ;
    addCustomWidget(fileDateLabel);
}

void GenericErrorItemWidget::openFolder(const QString &path) {
    const auto syncInfoMapIt = _gui->syncInfoMap().find(_errorInfo.syncDbId());
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder path %1.").arg(path), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    if (openInWebview()) {
        // Open in webview instead
        const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt != _gui->driveInfoMap().end()) {
            _gui->onOpenWebviewItem(syncInfoMapIt->second.driveDbId(), _errorInfo.remoteNodeId());
            return;
        }
    }

    // Open on local filesystem
    QString fullPath = syncInfoMapIt->second.localPath() + "/" + path;
    AbstractFileItemWidget::openFolder(fullPath);
}

const bool GenericErrorItemWidget::openInWebview() {
    return _errorInfo.inconsistencyType() == InconsistencyTypePathLength
        || _errorInfo.inconsistencyType() == InconsistencyTypeCase
        || _errorInfo.inconsistencyType() == InconsistencyTypeForbiddenChar
        || _errorInfo.inconsistencyType() == InconsistencyTypeReservedName
        || _errorInfo.inconsistencyType() == InconsistencyTypeNameLength
        || _errorInfo.inconsistencyType() == InconsistencyTypeNotYetSupportedChar
        || _errorInfo.cancelType() == CancelTypeAlreadyExistLocal
        || (_errorInfo.conflictType() == ConflictTypeEditDelete && !_errorInfo.remoteNodeId().isEmpty())
        || (_errorInfo.exitCode() == ExitCodeBackError && _errorInfo.exitCause() == ExitCauseNotFound);
}

}  // namespace KDC
