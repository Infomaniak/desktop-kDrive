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

#include "synchronizeditemwidget.h"
#include "menuitemwidget.h"
#include "menuwidget.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"

#include <QApplication>
#include <QBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QWidgetAction>

namespace KDC {

static const int cornerRadius = 10;
static const int hMargin = 15;
static const int vMargin = 5;
static const int boxHMargin = 12;
static const int boxVMargin = 5;
static const int boxSpacing = 12;
static const int toolBarHSpacing = 10;
static const int buttonsVSpacing = 5;
static const int textSpacing = 10;
static const int statusIconWidth = 10;
static const int shadowBlurRadius = 20;
static const QString dateFormat = "d MMM - HH:mm";
static const int fileNameMaxSize = 32;
static const int hoverStartTimer = 250;

Q_LOGGING_CATEGORY(lcSynchronizedItemWidget, "gui.synchronizeditemidget", QtInfoMsg)

SynchronizedItemWidget::SynchronizedItemWidget(const SynchronizedItem &item, QWidget *parent) :
    QWidget(parent),
    _item(item),
    _isWaitingTimer(false),
    _isSelected(false),
    _isMenuOpened(false),
    _cannotSelect(false),
    _fileIconSize(QSize()),
    _directionIconSize(QSize()),
    _backgroundColorSelection(QColor()),
    _fileIconLabel(nullptr),
    _fileDateLabel(nullptr) {
    setContentsMargins(hMargin, vMargin, hMargin, vMargin);

    _waitingTimer.setSingleShot(true);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    hbox->setSpacing(boxSpacing);
    setLayout(hbox);

    _fileIconLabel = new QLabel(this);
    hbox->addWidget(_fileIconLabel);

    QVBoxLayout *vboxText = new QVBoxLayout();
    vboxText->setContentsMargins(0, 0, 0, 0);
    vboxText->setSpacing(0);

    QLabel *fileNameLabel = new QLabel(this);
    fileNameLabel->setObjectName("fileNameLabel");
    QFileInfo fileInfo(_item.filePath());
    QString fileName = fileInfo.fileName();
    GuiUtility::makePrintablePath(fileName, fileNameMaxSize);
    fileNameLabel->setText(fileName);
    vboxText->addStretch();
    vboxText->addWidget(fileNameLabel);

    QHBoxLayout *hboxText = new QHBoxLayout();
    hboxText->setContentsMargins(0, 0, 0, 0);
    hboxText->setSpacing(textSpacing);

    _fileDateLabel = new QLabel(this);
    _fileDateLabel->setObjectName("fileDateLabel");
    _fileDateLabel->setText(_item.dateTime().toString(dateFormat));
    hboxText->addWidget(_fileDateLabel);

    _fileDirectionLabel = new QLabel(this);
    _fileDirectionLabel->setVisible(false);
    hboxText->addWidget(_fileDirectionLabel);
    hboxText->addStretch();

    vboxText->addLayout(hboxText);
    vboxText->addStretch();

    hbox->addLayout(vboxText);
    hbox->addStretch();

    QVBoxLayout *vboxButtons = new QVBoxLayout();
    vboxButtons->setContentsMargins(0, 0, 0, 0);
    vboxButtons->addSpacing(buttonsVSpacing);

    QHBoxLayout *hboxButtons = new QHBoxLayout();
    hboxButtons->setContentsMargins(0, 0, 0, 0);
    hboxButtons->setSpacing(toolBarHSpacing);

    _folderButton = new CustomToolButton(this);
    _folderButton->setIconPath(":/client/resources/icons/actions/folder.svg");
    _folderButton->setVisible(false);
    hboxButtons->addWidget(_folderButton);

    _menuButton = new CustomToolButton(this);
    _menuButton->setIconPath(":/client/resources/icons/actions/menu.svg");
    _menuButton->setVisible(false);
    hboxButtons->addWidget(_menuButton);

    vboxButtons->addLayout(hboxButtons);
    vboxButtons->addStretch();

    hbox->addLayout(vboxButtons);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &SynchronizedItemWidget::retranslateUi);

    connect(this, &SynchronizedItemWidget::fileIconSizeChanged, this, &SynchronizedItemWidget::onFileIconSizeChanged);
    connect(this, &SynchronizedItemWidget::directionIconSizeChanged, this, &SynchronizedItemWidget::onDirectionIconSizeChanged);
    connect(this, &SynchronizedItemWidget::directionIconColorChanged, this, &SynchronizedItemWidget::onDirectionIconColorChanged);
    connect(_folderButton, &CustomToolButton::clicked, this, &SynchronizedItemWidget::onFolderButtonClicked);
    connect(_menuButton, &CustomToolButton::clicked, this, &SynchronizedItemWidget::onMenuButtonClicked);
    connect(&_waitingTimer, &QTimer::timeout, this, &SynchronizedItemWidget::onWaitingTimerTimeout);
}

void SynchronizedItemWidget::onCannotSelect(bool cannotSelect) {
    _cannotSelect = cannotSelect;
}

void SynchronizedItemWidget::setSelected(bool isSelected) {
    if (_cannotSelect && isSelected) {
        return;
    }

    _isSelected = isSelected;
    if (_isSelected) {
        bool fileExists = QFile(_item.fullFilePath()).exists();
        _folderButton->setVisible(fileExists);
        _menuButton->setVisible(fileExists);
        _fileDirectionLabel->setVisible(true);

        // Shadow
        QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
        effect->setBlurRadius(shadowBlurRadius);
        effect->setOffset(0);
        effect->setColor(KDC::GuiUtility::getShadowColor());
        setGraphicsEffect(effect);
    } else {
        _folderButton->setVisible(false);
        _menuButton->setVisible(false);
        _fileDirectionLabel->setVisible(false);

        setGraphicsEffect(nullptr);
    }

    emit selectionChanged(isSelected);
}

void SynchronizedItemWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    // Update shadow color
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (effect) {
        effect->setColor(KDC::GuiUtility::getShadowColor());
    }

    if (_isSelected) {
        // Draw round rectangle
        QPainterPath painterPath;
        painterPath.addRoundedRect(rect().marginsRemoved(QMargins(hMargin, vMargin, hMargin, vMargin)), cornerRadius,
                                   cornerRadius);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(backgroundColorSelection());
        painter.drawPath(painterPath);
    }
}

void SynchronizedItemWidget::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)

    if (!_isWaitingTimer) {
        _isWaitingTimer = true;
        _waitingTimer.start(hoverStartTimer);
    }
}

void SynchronizedItemWidget::leaveEvent(QEvent *event) {
    Q_UNUSED(event)

    if (_isWaitingTimer) {
        _isWaitingTimer = false;
        if (_isSelected && !_isMenuOpened) {
            setSelected(false);
        }
    }
}

bool SynchronizedItemWidget::event(QEvent *event) {
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Hide) {
        setSelected(false);
    }
    return QWidget::event(event);
}

QIcon SynchronizedItemWidget::getIconWithStatus(const QString &filePath, NodeType type, SyncFileStatus status) {
    QGraphicsSvgItem *fileItem = new QGraphicsSvgItem(CommonUtility::getFileIconPathFromFileName(filePath, type));
    QGraphicsSvgItem *statusItem = new QGraphicsSvgItem(KDC::GuiUtility::getFileStatusIconPath(status));

    QGraphicsScene scene;
    scene.setSceneRect(QRectF(QPointF(0, 0), _fileIconSize));

    scene.addItem(fileItem);
    fileItem->setPos(QPointF(statusIconWidth / 2, statusIconWidth / 2));

    scene.addItem(statusItem);
    statusItem->setPos(QPointF(_fileIconSize.width() - statusIconWidth, _fileIconSize.width() - statusIconWidth));
    qreal statusItemScale = statusIconWidth / statusItem->boundingRect().width();
    statusItem->setScale(statusItemScale);

    Q_CHECK_PTR(qApp->primaryScreen());
    qreal ratio = qApp->primaryScreen()->devicePixelRatio();
    QPixmap pixmap(QSize(static_cast<int>(round(scene.width() * ratio)), static_cast<int>(round(scene.height() * ratio))));
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    scene.render(&painter);

    QIcon iconWithStatus;
    iconWithStatus.addPixmap(pixmap);

    return iconWithStatus;
}

void SynchronizedItemWidget::setDirectionIcon() {
    if (_fileDirectionLabel && _directionIconSize != QSize() && _directionIconColor != QColor()) {
        switch (_item.direction()) {
            case SyncDirection::Unknown:
                break;
            case SyncDirection::Up:
                _fileDirectionLabel->setPixmap(
                        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/upload.svg", _directionIconColor)
                                .pixmap(_directionIconSize));
                break;
            case SyncDirection::Down:
                _fileDirectionLabel->setPixmap(
                        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/download.svg", _directionIconColor)
                                .pixmap(_directionIconSize));
                break;
            case SyncDirection::EnumEnd: {
                assert(false && "Invalid enum value in switch statement.");
            }
        }
    }
}

void SynchronizedItemWidget::onFileIconSizeChanged() {
    if (_fileIconLabel) {
        QFileInfo fileInfo(_item.fullFilePath());
        QIcon fileIconWithStatus = getIconWithStatus(fileInfo.fileName(), _item.type(), _item.status());
        _fileIconLabel->setPixmap(fileIconWithStatus.pixmap(_fileIconSize));
    }
}

void SynchronizedItemWidget::onDirectionIconSizeChanged() {
    setDirectionIcon();
}

void SynchronizedItemWidget::onDirectionIconColorChanged() {
    setDirectionIcon();
}

void SynchronizedItemWidget::onFolderButtonClicked() {
    emit openFolder(_item);
    MatomoClient::sendEvent("synchronizedItem", MatomoEventAction::Click, "folderButton");
}

void SynchronizedItemWidget::onMenuButtonClicked() {
    if (_menuButton) {
        _isMenuOpened = true;
        MatomoClient::sendEvent("synchronizedItem", MatomoEventAction::Click, "openKebabMenu");
        auto *menu = new MenuWidget(MenuWidget::Menu, this);

        auto *openAction = new QWidgetAction(this);
        auto *openMenuItemWidget = new MenuItemWidget(tr("Open"));
        openMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/view.svg");
        openAction->setDefaultWidget(openMenuItemWidget);
        connect(openAction, &QWidgetAction::triggered, this, &SynchronizedItemWidget::onOpenActionTriggered);
        menu->addAction(openAction);

        if (!_item.fileId().isEmpty()) {
            auto *favoritesAction = new QWidgetAction(this);
            auto *favoritesMenuItemWidget = new MenuItemWidget(tr("Add to favorites"));
            favoritesMenuItemWidget->hide();
            favoritesMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/favorite.svg");
            favoritesAction->setDefaultWidget(favoritesMenuItemWidget);
            connect(favoritesAction, &QWidgetAction::triggered, this, &SynchronizedItemWidget::onFavoritesActionTriggered);
            menu->addAction(favoritesAction);

            auto *copyLinkAction = new QWidgetAction(this);
            auto *copyLinkMenuItemWidget = new MenuItemWidget(tr("Copy share link"));
            copyLinkMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/link.svg");
            copyLinkAction->setDefaultWidget(copyLinkMenuItemWidget);
            connect(copyLinkAction, &QWidgetAction::triggered, this, &SynchronizedItemWidget::onCopyLinkActionTriggered);
            menu->addAction(copyLinkAction);

            menu->addSeparator();

            auto *displayOnDriveAction = new QWidgetAction(this);
            auto *displayOnDriveMenuItemWidget = new MenuItemWidget(tr("Display on kdrive.infomaniak.com"));
            displayOnDriveMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/webview.svg");
            displayOnDriveAction->setDefaultWidget(displayOnDriveMenuItemWidget);
            connect(displayOnDriveAction, &QWidgetAction::triggered, this,
                    &SynchronizedItemWidget::onDisplayOnDriveActionTriggered);
            menu->addAction(displayOnDriveAction);
        }
        menu->exec(QWidget::mapToGlobal(_menuButton->geometry().center()));
        _isMenuOpened = false;
    }
}

void SynchronizedItemWidget::onOpenActionTriggered(bool checked) {
    Q_UNUSED(checked)

    MatomoClient::sendEvent("synchronizedItem", MatomoEventAction::Click, "openButton");
    emit open(_item);
}

void SynchronizedItemWidget::onFavoritesActionTriggered(bool checked) {
    Q_UNUSED(checked)

    emit addToFavourites(_item);
}

/*void SynchronizedItemWidget::onRightAndSharingActionTriggered(bool checked)
{
    Q_UNUSED(checked)

    emit manageRightAndSharing(_item);
}*/

void SynchronizedItemWidget::onCopyLinkActionTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("synchronizedItem", MatomoEventAction::Click, "copyLinkButton");
    emit copyLink(_item);
}

void SynchronizedItemWidget::onDisplayOnDriveActionTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("synchronizedItem", MatomoEventAction::Click, "displayOnDriveButton");
    emit displayOnWebview(_item);
}

void SynchronizedItemWidget::retranslateUi() {
    _folderButton->setToolTip(tr("Show in folder"));
    _menuButton->setToolTip(tr("More actions"));
    _fileDateLabel->setText(GuiUtility::getDateForCurrentLanguage(_item.dateTime(), dateFormat));
}

void SynchronizedItemWidget::onWaitingTimerTimeout() {
    if (_isWaitingTimer) {
        setSelected(true);
    }
}


} // namespace KDC
