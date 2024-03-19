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

#include "synthesisitemwidget.h"

#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPoint>

namespace KDC {

static const int cornerRadius = 10;
static const int widgetMarginX = 15;
static const int iconPositionX = 12;
static const int iconWidth = 25;
static const int iconHeight = 25;

SynthesisItemWidget::SynthesisItemWidget(const SynchronizedItem &item, QWidget *parent) : QWidget(parent), _item(item) {}

void SynthesisItemWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect().marginsRemoved(QMargins(widgetMarginX, 0, widgetMarginX, 0)), cornerRadius, cornerRadius);
    painterPath.addText(QPointF(20, rect().height() / 2.0), option.font, _item.name());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);
    painter.drawPath(painterPath);

    // Draw icon
    QFileInfo fileInfo(_item.name());
    QIcon fileIcon = getIconFromFileName(fileInfo.fileName());
    QRectF fileIconRect = QRectF(rect().left() + widgetMarginX + iconPositionX,
                                 rect().top() + (rect().height() - iconHeight) / 2.0, iconWidth, iconHeight);
    QPixmap fileIconPixmap = fileIcon.pixmap(iconWidth, iconHeight, QIcon::Normal);
    painter.drawPixmap(QPoint(fileIconRect.left(), fileIconRect.top()), fileIconPixmap);
}

QIcon SynthesisItemWidget::getIconFromFileName(const QString &fileName) const {
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
    if (mime.name().startsWith("image/")) {
        return QIcon(":/client/resources/icons/document types/file-image");
    } else if (mime.name().startsWith("audio/")) {
        return QIcon(":/client/resources/icons/document types/file-audio");
    } else if (mime.name().startsWith("video/")) {
        return QIcon(":/client/resources/icons/document types/file-video");
    } else if (mime.inherits("application/pdf")) {
        return QIcon(":/client/resources/icons/document types/file-pdf");
    } else if (mime.name().startsWith("application/vnd.ms-powerpoint") ||
               mime.name().startsWith("application/vnd.openxmlformats-officedocument.presentationml") ||
               mime.inherits("application/vnd.oasis.opendocument.presentation")) {
        return QIcon(":/client/resources/icons/document types/file-presentation");
    } else if (mime.name().startsWith("application/vnd.ms-excel") ||
               mime.name().startsWith("application/vnd.openxmlformats-officedocument.spreadsheetml") ||
               mime.inherits("application/vnd.oasis.opendocument.spreadsheet")) {
        return QIcon(":/client/resources/icons/document types/file-sheets");
    } else if (mime.inherits("application/zip") || mime.inherits("application/gzip") || mime.inherits("application/tar") ||
               mime.inherits("application/rar") || mime.inherits("application/x-bzip2")) {
        return QIcon(":/client/resources/icons/document types/file-zip");
    } else if (mime.inherits("text/x-csrc") || mime.inherits("text/x-c++src") || mime.inherits("text/x-java") ||
               mime.inherits("text/x-objcsrc") || mime.inherits("text/x-python") || mime.inherits("text/asp") ||
               mime.inherits("text/html") || mime.inherits("text/javascript") || mime.inherits("application/x-php") ||
               mime.inherits("application/x-perl")) {
        return QIcon(":/client/resources/icons/document types/file-code");
    } else if (mime.inherits("text/plain") || mime.inherits("text/xml")) {
        return QIcon(":/client/resources/icons/document types/file-text");
    } else if (mime.inherits("application/x-msdos-program")) {
        return QIcon(":/client/resources/icons/document types/file-application");
    }

    return QIcon(":/client/resources/icons/document types/file-default");
}

}  // namespace KDC
