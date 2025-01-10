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

#include "fileerroritemwidget.h"
#include "guiutility.h"
#include "libcommongui/utility/utility.h"

#include <QBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>

namespace KDC {

static const int vMargin = 5;
static const int boxVMargin = 15;
static const int boxHSpacing = 10;
static const int boxVSpacing = 10;
static const int shadowBlurRadius = 20;
static const QSize statusIconSize = QSize(15, 15);
static const int fileErrorLabelMaxWidth = 375;
static const int fileNameMaxSize = 40;
static const QString dateFormat = "d MMM yyyy - HH:mm";

Q_LOGGING_CATEGORY(lcFileErrorItemWidget, "gui.fileerroritemwidget", QtInfoMsg)

FileErrorItemWidget::FileErrorItemWidget(const SynchronizedItem &item, const DriveInfo &driveInfo, QWidget *parent) :
    GenericErrorItemWidget(parent), _item(item), _fileNameLabel(nullptr) {
    QHBoxLayout *mainLayout = qobject_cast<QHBoxLayout *>(layout());

    // Left box
    QVBoxLayout *vBoxLeft = new QVBoxLayout();
    vBoxLeft->setContentsMargins(0, 0, 0, 0);
    // vBoxLeft->setMargin(0);
    mainLayout->addLayout(vBoxLeft);

    QLabel *statusIconLabel = new QLabel(this);
    QString statusIconPath = KDC::GuiUtility::getFileStatusIconPath(_item.status());
    statusIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(statusIconPath).pixmap(statusIconSize));
    vBoxLeft->addWidget(statusIconLabel);
    vBoxLeft->addStretch();

    // Middle box
    QVBoxLayout *vBoxMiddle = new QVBoxLayout();
    vBoxMiddle->setContentsMargins(0, 0, 0, 0);
    vBoxMiddle->setSpacing(boxVSpacing);
    mainLayout->addLayout(vBoxMiddle);
    mainLayout->setStretchFactor(vBoxMiddle, 1);

    _fileNameLabel = new QLabel(this);
    _fileNameLabel->setObjectName("fileNameLabel");
    QFileInfo fileInfo(_item.filePath());
    QString fileName = fileInfo.fileName();
    GuiUtility::makePrintablePath(fileName, fileNameMaxSize);
    _fileNameLabel->setText(fileName);
    _fileNameLabel->setWordWrap(true);
    vBoxMiddle->addWidget(_fileNameLabel);

    _errorLabel = new CustomWordWrapLabel(this);
    _errorLabel->setObjectName("errorLabel");
    _errorLabel->setMaxWidth(fileErrorLabelMaxWidth);
    _errorLabel->setText(_item.error());
    _errorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vBoxMiddle->addWidget(_errorLabel);
    vBoxMiddle->setStretchFactor(_errorLabel, 1);

    QHBoxLayout *hBoxFilePath = new QHBoxLayout();
    hBoxFilePath->setContentsMargins(0, 0, 0, 0);
    hBoxFilePath->setSpacing(boxHSpacing);
    vBoxMiddle->addLayout(hBoxFilePath);

    QLabel *driveIconLabel = new QLabel(this);
    driveIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/drive.svg", driveInfo.color())
                                      .pixmap(statusIconSize));
    hBoxFilePath->addWidget(driveIconLabel);

    _pathLabel = new QLabel(this);
    _pathLabel->setObjectName("filePathLabel");
    QString filePath = driveInfo.name() + dirSeparator + fileInfo.path();
    GuiUtility::makePrintablePath(filePath);
    _pathLabel->setText(QString("<a style=\"%1\" href=\"ref\">%2</a>").arg(CommonUtility::linkStyle, filePath));
    _pathLabel->setWordWrap(true);
    _pathLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    hBoxFilePath->addWidget(_pathLabel);
    hBoxFilePath->setStretchFactor(_pathLabel, 1);

    // Right box
    QVBoxLayout *vBoxRight = new QVBoxLayout();
    vBoxRight->setContentsMargins(0, 0, 0, 0);
    vBoxRight->setSpacing(0);
    mainLayout->addLayout(vBoxRight);

    QLabel *fileDateLabel = new QLabel(this);
    fileDateLabel->setObjectName("fileDateLabel");
    fileDateLabel->setText(_item.dateTime().toString(dateFormat));
    vBoxRight->addWidget(fileDateLabel);
    vBoxRight->addStretch();

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    connect(_pathLabel, &QLabel::linkActivated, this, &FileErrorItemWidget::onLinkActivated);
}

QSize FileErrorItemWidget::sizeHint() const {
    int height = 2 * vMargin + 2 * boxVMargin + 2 * boxVSpacing + _fileNameLabel->sizeHint().height() +
                 _errorLabel->sizeHint().height() + qMax(statusIconSize.height(), _pathLabel->sizeHint().height());
    return QSize(width(), height);
}

void FileErrorItemWidget::onLinkActivated(const QString &link) {
    Q_UNUSED(link)

    emit openFolder(_item.fullFilePath());
}

} // namespace KDC
