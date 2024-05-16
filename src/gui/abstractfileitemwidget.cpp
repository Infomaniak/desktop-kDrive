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

#include "abstractfileitemwidget.h"
#include "gui/custommessagebox.h"
#include "guiutility.h"

#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QDir>

#include <utility/utility.h>

namespace KDC {

static const int cornerRadius = 10;
static const int hMargin = 20;
static const int vMargin = 5;
static const int boxVSpacing = 10;
static const int shadowBlurRadius = 20;
static const QSize iconSize = QSize(15, 15);

AbstractFileItemWidget::AbstractFileItemWidget(QWidget *parent /*= nullptr*/)
    : QWidget(parent)
    , _topLayout(new QHBoxLayout)
    , _fileTypeIconLabel(new QLabel)
    , _filenameLabel(new QLabel)
    , _middleLayout(new QHBoxLayout)
    , _messageLabel(new QLabel)
    , _bottomLayout(new QHBoxLayout)
    , _driveIconLabel(new QLabel)
    , _pathLabel(new QLabel)
{
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);

    auto mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(boxVSpacing);
    setLayout(mainLayout);

    // Top layout
    _topLayout->setContentsMargins(0, 0, 0, 0);
    _topLayout->setAlignment(Qt::AlignVCenter);

    _fileTypeIconLabel->setObjectName("fileNameLabel");
    _fileTypeIconLabel->setMinimumSize(iconSize);
    _fileTypeIconLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    _topLayout->addWidget(_fileTypeIconLabel);

    _filenameLabel->setObjectName("fileNameLabel");
    _filenameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _filenameLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    _filenameLabel->setMinimumHeight(16);
    _topLayout->addWidget(_filenameLabel);

    _topLayout->addStretch();

    mainLayout->addLayout(_topLayout);

    // Middle layout
    _middleLayout->setContentsMargins(0, 0, 0, 0);
    _middleLayout->setAlignment(Qt::AlignVCenter);

    _messageLabel->setObjectName("errorLabel");
    _messageLabel->setWordWrap(true);
    _messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    _messageLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    _middleLayout->addWidget(_messageLabel);

    // Bottom layout
    _bottomLayout->setContentsMargins(0, 0, 0, 0);
    _bottomLayout->setAlignment(Qt::AlignVCenter);

    _driveIconLabel->setObjectName("errorLabel");
    _driveIconLabel->setMinimumSize(iconSize);
    _driveIconLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    _bottomLayout->addWidget(_driveIconLabel);

    _pathLabel->setObjectName("filePathLabel");
    _pathLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    _pathLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    _pathLabel->setMinimumHeight(16);
    _bottomLayout->addWidget(_pathLabel);

    _bottomLayout->addStretch();

    mainLayout->addLayout(_bottomLayout);

    // Shadow
    auto effect = new QGraphicsDropShadowEffect;
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    setGraphicsEffect(effect);

    // Generic error icon by default
    setFileTypeIcon(":/client/resources/icons/statuts/error-sync.svg");

    setMinimumHeight(80);

    connect(_pathLabel, &QLabel::linkActivated, this, &AbstractFileItemWidget::openFolder);
}

QSize AbstractFileItemWidget::sizeHint() const {
    int height = _filenameLabel->sizeHint().height() + _messageLabel->sizeHint().height() + _pathLabel->sizeHint().height() +
                 2 * boxVSpacing;

    return {width(), height};
}

void AbstractFileItemWidget::setFilePath(const QString &filePath, NodeType type /*= NodeTypeFile*/) {
    setFileName(filePath, type);
    setPath(filePath);
}

void AbstractFileItemWidget::setDriveName(const QString &driveName, const QString &localPath) {
    QString str = QString("<a style=\"%1\" href=\"%2\">%3</a>").arg(CommonUtility::linkStyle, localPath, driveName);
    _pathLabel->setText(str);
}

void AbstractFileItemWidget::setPathIconColor(const QColor &color) {
    _driveIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/icon-folder-empty.svg", color).pixmap(iconSize));
}

void AbstractFileItemWidget::setMessage(const QString &str) {
    _messageLabel->setText(str);
    auto mainLayout = qobject_cast<QVBoxLayout *>(layout());
    mainLayout->insertLayout(1, _middleLayout);
}

void AbstractFileItemWidget::addCustomWidget(QWidget *w) {
    _topLayout->addWidget(w);
}

void AbstractFileItemWidget::openFolder(const QString &path) {
    if (!KDC::GuiUtility::openFolder(path)) {
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder path %1.").arg(path), QMessageBox::Ok, this);
        msgBox.exec();
    }
}

void AbstractFileItemWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _driveIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/icon-folder-empty.svg", _logoColor).pixmap(iconSize));
}

void AbstractFileItemWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Update shadow color
    auto effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor());
    }

    // Draw round rectangle
    QPainterPath painterPath;
    painterPath.addRoundedRect(rect().marginsRemoved(QMargins(hMargin, vMargin, hMargin, vMargin)), cornerRadius, cornerRadius);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(_backgroundColor);
    painter.drawPath(painterPath);
}

void AbstractFileItemWidget::setFileTypeIcon(const QString &ressourcePath) {
    _fileTypeIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(ressourcePath).pixmap(iconSize));
}

void AbstractFileItemWidget::setFileName(const QString &path, NodeType type) {
    setFileTypeIcon(CommonUtility::getFileIconPathFromFileName(path, type));
    _filenameLabel->setText(QFileInfo(path).fileName());
}

void AbstractFileItemWidget::setPath(const QString &path) {
    _driveIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/icon-folder-empty.svg", _logoColor).pixmap(iconSize));

    const QFileInfo fInfo(path);
    QString printablePath;
    if (!fInfo.isAbsolute()) printablePath = "/";
    printablePath += path;
    GuiUtility::makePrintablePath(printablePath);

    printablePath = QDir::toNativeSeparators(printablePath);
    QString pathStr = QString("<a style=\"%1\" href=\"%2\">%3</a>").arg(CommonUtility::linkStyle, path, printablePath);

    _pathLabel->setText(pathStr);
    _pathLabel->setToolTip(path);
}

}  // namespace KDC
