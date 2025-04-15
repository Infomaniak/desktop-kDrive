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

GenericErrorItemWidget::GenericErrorItemWidget(std::shared_ptr<ClientGui> gui, const QString &errorMsg,
                                               const ErrorInfo &errorInfo, QWidget *parent) :
    AbstractFileItemWidget(parent), _gui(gui), _errorInfo(errorInfo), _errorMsg(errorMsg) {
    init();
}

void GenericErrorItemWidget::init() {
    setMessage(_errorMsg);

    // Path layout
    if (_errorInfo.level() == ErrorLevel::SyncPal || _errorInfo.level() == ErrorLevel::Node) {
        const auto &syncInfoMapIt = _gui->syncInfoMap().find(_errorInfo.syncDbId());
        if (syncInfoMapIt == _gui->syncInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        const auto &driveInfoMapIt = _gui->driveInfoMap().find(syncInfoMapIt->second.driveDbId());
        if (driveInfoMapIt == _gui->driveInfoMap().end()) {
            throw std::runtime_error(GENERICERRORITEMWIDGET_NEW_ERROR_MSG);
        }

        // Path
        if (_errorInfo.level() == ErrorLevel::SyncPal) {
            setDriveName(driveInfoMapIt->second.name(), syncInfoMapIt->second.localPath());
            setPathIconColor(driveInfoMapIt->second.color());
        } else if (_errorInfo.level() == ErrorLevel::Node) {
            const bool useDestPath = _errorInfo.cancelType() == CancelType::AlreadyExistRemote ||
                                     _errorInfo.cancelType() == CancelType::MoveToBinFailed ||
                                     _errorInfo.cancelType() == CancelType::FileRescued ||
                                     _errorInfo.conflictType() == ConflictType::EditDelete;
            const QString &filePath = useDestPath ? _errorInfo.destinationPath() : _errorInfo.path();
            setPathAndName(filePath, _errorInfo.nodeType());
        }
    }

    // Right layout
    auto fileDateLabel = new QLabel(this);
    fileDateLabel->setObjectName("fileDateLabel");
    const auto errorTime = _errorInfo.getTime();
    const QDateTime dateTime = errorTime ? QDateTime::fromSecsSinceEpoch(errorTime)
                                         : QDateTime::currentDateTime(); // If error time is not set, use current time.
    fileDateLabel->setText(GuiUtility::getDateForCurrentLanguage(dateTime, dateFormat));

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

    // Open on local filesystem (open the parent folder for an item of file type).
    const auto folderPath = GuiUtility::getFolderPath(syncInfoMapIt->second.localPath() + "/" + path, _errorInfo.nodeType());
    AbstractFileItemWidget::openFolder(folderPath);
}

bool GenericErrorItemWidget::openInWebview() const {
    return _errorInfo.inconsistencyType() == InconsistencyType::PathLength ||
           _errorInfo.inconsistencyType() == InconsistencyType::Case ||
           _errorInfo.inconsistencyType() == InconsistencyType::ForbiddenChar ||
           _errorInfo.inconsistencyType() == InconsistencyType::ReservedName ||
           _errorInfo.inconsistencyType() == InconsistencyType::NameLength ||
           _errorInfo.inconsistencyType() == InconsistencyType::NotYetSupportedChar ||
           _errorInfo.cancelType() == CancelType::AlreadyExistLocal ||
           (_errorInfo.conflictType() == ConflictType::EditDelete && !_errorInfo.remoteNodeId().isEmpty()) ||
           (_errorInfo.exitCode() == ExitCode::BackError && _errorInfo.exitCause() == ExitCause::NotFound);
}

} // namespace KDC
