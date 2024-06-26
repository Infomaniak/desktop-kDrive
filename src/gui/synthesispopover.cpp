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

#include "clientgui.h"
#include "synthesispopover.h"
#include "menuwidget.h"
#include "bottomwidget.h"
#include "customtogglepushbutton.h"
#include "synchronizeditem.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "parameterscache.h"
#include "config.h"
#include "libcommon/utility/utility.h"

#include "updater/updaterclient.h"

#undef CONSOLE_DEBUG
#ifdef CONSOLE_DEBUG
#include <iostream>
#endif

#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLoggingCategory>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QScreen>
#include <QScrollBar>
#include <QPushButton>

#define UPDATE_PROGRESS_DELAY 500

namespace KDC {

static const QSize windowSize(440, 575);
static const QSize lockedWindowSize(440, 400);
static const int triangleHeight = 10;
static const int triangleWidth = 20;
static const int trianglePosition = 100;  // Position from side
static const int cornerRadius = 5;
static const int shadowBlurRadius = 20;
static const int toolBarHMargin = 10;
static const int toolBarVMargin = 10;
static const int toolBarSpacing = 10;
static const int driveBoxHMargin = 10;
static const int driveBoxVMargin = 10;
static const int defaultPageSpacing = 20;
static const int logoIconSize = 30;
static const int defaultLogoIconSize = 50;
static const int maxSynchronizedItems = 50;

const std::map<NotificationsDisabled, QString> SynthesisPopover::_notificationsDisabledMap = {
    {NotificationsDisabledNever, QString(tr("Never"))},
    {NotificationsDisabledOneHour, QString(tr("During 1 hour"))},
    {NotificationsDisabledUntilTomorrow, QString(tr("Until tomorrow 8:00AM"))},
    {NotificationsDisabledTreeDays, QString(tr("During 3 days"))},
    {NotificationsDisabledOneWeek, QString(tr("During 1 week"))},
    {NotificationsDisabledAlways, QString(tr("Always"))}};

const std::map<NotificationsDisabled, QString> SynthesisPopover::_notificationsDisabledForPeriodMap = {
    {NotificationsDisabledNever, QString(tr("Never"))},
    {NotificationsDisabledOneHour, QString(tr("For 1 more hour"))},
    {NotificationsDisabledUntilTomorrow, QString(tr("Until tomorrow 8:00AM"))},
    {NotificationsDisabledTreeDays, QString(tr("For 3 more days"))},
    {NotificationsDisabledOneWeek, QString(tr("For 1 more week"))},
    {NotificationsDisabledAlways, QString(tr("Always"))}};

Q_LOGGING_CATEGORY(lcSynthesisPopover, "gui.synthesispopover", QtInfoMsg)

SynthesisPopover::SynthesisPopover(std::shared_ptr<ClientGui> gui, bool debugMode, QWidget *parent)
    : QDialog(parent), _gui(gui), _debugMode(debugMode) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setMinimumSize(windowSize);
    setMaximumSize(windowSize);

    _notificationsDisabled = ParametersCache::instance()->parametersInfo().notificationsDisabled();

    initUI();

    connect(this, &SynthesisPopover::updateItemList, this, &SynthesisPopover::onUpdateSynchronizedListWidget,
            Qt::QueuedConnection);
}

SynthesisPopover::~SynthesisPopover() {}

void SynthesisPopover::setPosition(const QRect &sysTrayIconRect) {
    _sysTrayIconRect = sysTrayIconRect;

    QScreen *screen = QGuiApplication::screenAt(_sysTrayIconRect.center());
    if (!screen) {
        qCDebug(lcSynthesisPopover) << "No Screen found";
        return;
    }

    QRect screenRect = screen->availableGeometry();

    KDC::GuiUtility::systrayPosition position = KDC::GuiUtility::getSystrayPosition(screen);

    bool trianglePositionLeft = false;
    bool trianglePositionTop = false;
    if (_sysTrayIconRect != QRect() && KDC::GuiUtility::isPointInSystray(screen, _sysTrayIconRect.center())) {
        if (position == KDC::GuiUtility::systrayPosition::Top || position == KDC::GuiUtility::systrayPosition::Bottom) {
            // Triangle position (left/right)
            trianglePositionLeft =
                (_sysTrayIconRect.center().x() + rect().width() - trianglePosition < screenRect.x() + screenRect.width());
        } else if (position == KDC::GuiUtility::systrayPosition::Left || position == KDC::GuiUtility::systrayPosition::Right) {
            // Triangle position (top/bottom)
            trianglePositionTop =
                (_sysTrayIconRect.center().y() + rect().height() - trianglePosition < screenRect.y() + screenRect.height());
        }
    }

    // Move window
    QPoint popoverPosition;
    if (_sysTrayIconRect == QRect() || !KDC::GuiUtility::isPointInSystray(screen, _sysTrayIconRect.center())) {
        // Unknown Systray icon position (Linux) or icon not in Systray (Windows group)

        // Dialog position
        if (position == KDC::GuiUtility::systrayPosition::Top) {
            popoverPosition =
                QPoint(screenRect.x() + screenRect.width() - rect().width() - triangleHeight, screenRect.y() + triangleHeight);
        } else if (position == KDC::GuiUtility::systrayPosition::Bottom) {
            popoverPosition = QPoint(screenRect.x() + screenRect.width() - rect().width() - triangleHeight,
                                     screenRect.y() + screenRect.height() - rect().height() - triangleHeight);
        } else if (position == KDC::GuiUtility::systrayPosition::Left) {
            popoverPosition =
                QPoint(screenRect.x() + triangleHeight, screenRect.y() + screenRect.height() - rect().height() - triangleHeight);
        } else if (position == KDC::GuiUtility::systrayPosition::Right) {
            popoverPosition = QPoint(screenRect.x() + screenRect.width() - rect().width() - triangleHeight,
                                     screenRect.y() + screenRect.height() - rect().height() - triangleHeight);
        }
    } else {
        if (position == KDC::GuiUtility::systrayPosition::Top) {
            // Dialog position
            popoverPosition = QPoint(trianglePositionLeft ? _sysTrayIconRect.center().x() - trianglePosition
                                                          : _sysTrayIconRect.center().x() - rect().width() + trianglePosition,
                                     screenRect.y());
        } else if (position == KDC::GuiUtility::systrayPosition::Bottom) {
            // Dialog position
            popoverPosition = QPoint(trianglePositionLeft ? _sysTrayIconRect.center().x() - trianglePosition
                                                          : _sysTrayIconRect.center().x() - rect().width() + trianglePosition,
                                     screenRect.y() + screenRect.height() - rect().height());
        } else if (position == KDC::GuiUtility::systrayPosition::Left) {
            // Dialog position
            popoverPosition =
                QPoint(screenRect.x(), trianglePositionTop ? _sysTrayIconRect.center().y() - trianglePosition
                                                           : _sysTrayIconRect.center().y() - rect().height() + trianglePosition);
        } else if (position == KDC::GuiUtility::systrayPosition::Right) {
            // Dialog position
            popoverPosition = QPoint(screenRect.x() + screenRect.width() - rect().width(),
                                     trianglePositionTop ? _sysTrayIconRect.center().y() - trianglePosition
                                                         : _sysTrayIconRect.center().y() - rect().height() + trianglePosition);
        }
    }

    move(popoverPosition);
}

void SynthesisPopover::forceRedraw() {
#ifdef Q_OS_WINDOWS
    // Windows hack
    QTimer::singleShot(0, this, [=]() {
        if (geometry().height() > windowSize.height()) {
            setGeometry(geometry() + QMargins(0, 0, 0, -1));
            setMaximumHeight(windowSize.height());
        } else {
            setMaximumHeight(windowSize.height() + 1);
            setGeometry(geometry() + QMargins(0, 0, 0, 1));
        }
    });
#endif
}

void SynthesisPopover::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);

    switch (event->type()) {
        case QEvent::PaletteChange:
        case QEvent::ThemeChange:
            emit applyStyle();
            break;
        default:
            break;
    }
}

void SynthesisPopover::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QScreen *screen = QGuiApplication::screenAt(_sysTrayIconRect.center());
    if (!screen) {
        qCDebug(lcSynthesisPopover) << "No Screen found";
        return;
    }

    QRect screenRect = screen->availableGeometry();

    KDC::GuiUtility::systrayPosition position = KDC::GuiUtility::getSystrayPosition(screen);

    bool trianglePositionLeft = false;
    bool trianglePositionTop = false;
    if (_sysTrayIconRect != QRect() && KDC::GuiUtility::isPointInSystray(screen, _sysTrayIconRect.center())) {
        if (position == KDC::GuiUtility::systrayPosition::Top || position == KDC::GuiUtility::systrayPosition::Bottom) {
            // Triangle position (left/right)
            trianglePositionLeft =
                (_sysTrayIconRect.center().x() + rect().width() - trianglePosition < screenRect.x() + screenRect.width());
        } else if (position == KDC::GuiUtility::systrayPosition::Left || position == KDC::GuiUtility::systrayPosition::Right) {
            // Triangle position (top/bottom)
            trianglePositionTop =
                (_sysTrayIconRect.center().y() + rect().height() - trianglePosition < screenRect.y() + screenRect.height());
        }
    }

    // Calculate border path
    QPainterPath painterPath;
    QRect intRect = rect().marginsRemoved(QMargins(triangleHeight, triangleHeight, triangleHeight - 1, triangleHeight - 1));
    if (_sysTrayIconRect == QRect() || !KDC::GuiUtility::isPointInSystray(screen, _sysTrayIconRect.center())) {
        // Border
        painterPath.addRoundedRect(intRect, cornerRadius, cornerRadius);
    } else {
        int cornerDiameter = 2 * cornerRadius;
        QPointF trianglePoint1;
        QPointF trianglePoint2;
        QPointF trianglePoint3;
        if (position == KDC::GuiUtility::systrayPosition::Top) {
            // Triangle points
            trianglePoint1 = QPoint(trianglePositionLeft ? trianglePosition - triangleWidth / 2.0
                                                         : rect().width() - trianglePosition - triangleWidth / 2.0,
                                    triangleHeight);
            trianglePoint2 = QPoint(trianglePositionLeft ? trianglePosition : rect().width() - trianglePosition, 0);
            trianglePoint3 = QPoint(trianglePositionLeft ? trianglePosition + triangleWidth / 2.0
                                                         : rect().width() - trianglePosition + triangleWidth / 2.0,
                                    triangleHeight);

            // Border
            painterPath.moveTo(trianglePoint3);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint1);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.arcTo(
                QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 270,
                90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Bottom) {
            // Triangle points
            trianglePoint1 = QPoint(trianglePositionLeft ? trianglePosition - triangleWidth / 2.0
                                                         : rect().width() - trianglePosition - triangleWidth / 2.0,
                                    rect().height() - triangleHeight);
            trianglePoint2 = QPoint(trianglePositionLeft ? trianglePosition : rect().width() - trianglePosition, rect().height());
            trianglePoint3 = QPoint(trianglePositionLeft ? trianglePosition + triangleWidth / 2.0
                                                         : rect().width() - trianglePosition + triangleWidth / 2.0,
                                    rect().height() - triangleHeight);

            // Border
            painterPath.moveTo(trianglePoint1);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint3);
            painterPath.arcTo(
                QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 270,
                90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Left) {
            // Triangle points
            trianglePoint1 =
                QPoint(triangleHeight, trianglePositionTop ? trianglePosition - triangleWidth / 2.0
                                                           : rect().height() - trianglePosition - triangleWidth / 2.0);
            trianglePoint2 = QPoint(0, trianglePositionTop ? trianglePosition : rect().height() - trianglePosition);
            trianglePoint3 =
                QPoint(triangleHeight, trianglePositionTop ? trianglePosition + triangleWidth / 2.0
                                                           : rect().height() - trianglePosition + triangleWidth / 2.0);

            // Border
            painterPath.moveTo(trianglePoint1);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint3);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.arcTo(
                QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 270,
                90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Right) {
            // Triangle
            trianglePoint1 = QPoint(rect().width() - triangleHeight,
                                    trianglePositionTop ? trianglePosition - triangleWidth / 2.0
                                                        : rect().height() - trianglePosition - triangleWidth / 2.0);
            trianglePoint2 = QPoint(rect().width(), trianglePositionTop ? trianglePosition : rect().height() - trianglePosition);
            trianglePoint3 = QPoint(rect().width() - triangleHeight,
                                    trianglePositionTop ? trianglePosition + triangleWidth / 2.0
                                                        : rect().height() - trianglePosition + triangleWidth / 2.0);

            // Border
            painterPath.moveTo(trianglePoint3);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint1);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.arcTo(
                QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 270,
                90);
            painterPath.closeSubpath();
        }
    }

    // Update shadow color
    QGraphicsDropShadowEffect *effect = qobject_cast<QGraphicsDropShadowEffect *>(graphicsEffect());
    if (!effect) {
        effect = new QGraphicsDropShadowEffect(this);
    }
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    effect->setColor(KDC::GuiUtility::getShadowColor(true));
    setGraphicsEffect(effect);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(backgroundMainColor());
    painter.setPen(Qt::NoPen);
    painter.drawPath(painterPath);
}

bool SynthesisPopover::event(QEvent *event) {
    bool ret = QDialog::event(event);
    if (event->type() == QEvent::WindowDeactivate) {
        setGraphicsEffect(nullptr);
        done(QDialog::Accepted);
    } else if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
        if (_gui->isConnected()) {
            // Activate/deactivate requests
            _gui->activateLoadInfo(event->type() == QEvent::Show);

            if (event->type() == QEvent::Show) {
                refreshErrorsButton();
                // Update the list of synchronized items
                qCDebug(lcSynthesisPopover) << "Show event";
                emit updateItemList();
            }
        }
    }
    return ret;
}

void SynthesisPopover::initUI() {
    /*
     *  mainVBox
     *      hBoxToolBar
     *          iconLabel
     *          _errorsButton
     *          _infosButton
     *          _menuButton
     *      hBoxDriveBar
     *          _driveSelectionWidget
     *      _progressBarWidget
     *      _statusBarWidget
     *      _buttonsBarWidget
     *          synchronizedButton
     *          favoritesButton
     *          activityButton
     *      _stackedWidget
     *          _defaultSynchronizedPageWidget
     *          notImplementedLabel
     *          notImplementedLabel2
     *          _synchronizedListWidget[]
     *      bottomWidget
     */
    QVBoxLayout *appVBox = new QVBoxLayout();
    setLayout(appVBox);

    _mainWidget = new QWidget(this);
    _lockedAppVesrionWidget = new QWidget(this);
    _lockedAppVesrionWidget->hide();

    appVBox->addWidget(_mainWidget);
    appVBox->addWidget(_lockedAppVesrionWidget);

    QVBoxLayout *mainVBox = new QVBoxLayout(_mainWidget);
    mainVBox->setContentsMargins(triangleHeight, triangleHeight, triangleHeight, triangleHeight);
    mainVBox->setSpacing(0);
    // Tool bar
    QHBoxLayout *hBoxToolBar = new QHBoxLayout();
    hBoxToolBar->setContentsMargins(toolBarHMargin, toolBarVMargin, toolBarHMargin, toolBarVMargin);
    hBoxToolBar->setSpacing(toolBarSpacing);
    mainVBox->addLayout(hBoxToolBar);

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg").pixmap(logoIconSize, logoIconSize));
    hBoxToolBar->addWidget(iconLabel);

    hBoxToolBar->addStretch();

    _errorsButton = new CustomToolButton(this);
    _errorsButton->setObjectName("errorsButton");
    _errorsButton->setIconPath(":/client/resources/icons/actions/warning.svg");
    _errorsButton->setVisible(false);
    _errorsButton->setWithMenu(true);
    hBoxToolBar->addWidget(_errorsButton);

    _infosButton = new CustomToolButton(this);
    _infosButton->setObjectName("informationButton");
    _infosButton->setIconPath(":/client/resources/icons/actions/information.svg");
    _infosButton->setVisible(false);
    _infosButton->setWithMenu(true);
    hBoxToolBar->addWidget(_infosButton);

    _menuButton = new CustomToolButton(this);
    _menuButton->setIconPath(":/client/resources/icons/actions/menu.svg");
    hBoxToolBar->addWidget(_menuButton);

    // Drive selection
    QHBoxLayout *hBoxDriveBar = new QHBoxLayout();
    hBoxDriveBar->setContentsMargins(driveBoxHMargin, driveBoxVMargin, driveBoxHMargin, driveBoxVMargin);

    _driveSelectionWidget = new DriveSelectionWidget(_gui);
    hBoxDriveBar->addWidget(_driveSelectionWidget);

    hBoxDriveBar->addStretch();
    mainVBox->addLayout(hBoxDriveBar);

    // Progress bar
    _progressBarWidget = new ProgressBarWidget(this);
    mainVBox->addWidget(_progressBarWidget);

    // Status bar
    _statusBarWidget = new StatusBarWidget(_gui, this);
    mainVBox->addWidget(_statusBarWidget);

    // Buttons bar
    _buttonsBarWidget = new ButtonsBarWidget(this);
    _buttonsBarWidget->hide();
    mainVBox->addWidget(_buttonsBarWidget);

    CustomTogglePushButton *synchronizedButton = new CustomTogglePushButton(tr("Synchronized"), _buttonsBarWidget);
    synchronizedButton->setIconPath(":/client/resources/icons/actions/sync.svg");
    _buttonsBarWidget->insertButton(DriveInfoClient::SynthesisStackedWidgetSynchronized, synchronizedButton);

    CustomTogglePushButton *favoritesButton = new CustomTogglePushButton(tr("Favorites"), _buttonsBarWidget);
    favoritesButton->setIconPath(":/client/resources/icons/actions/favorite.svg");
    _buttonsBarWidget->insertButton(DriveInfoClient::SynthesisStackedWidgetFavorites, favoritesButton);

    CustomTogglePushButton *activityButton = new CustomTogglePushButton(tr("Activity"), _buttonsBarWidget);
    activityButton->setIconPath(":/client/resources/icons/actions/notifications.svg");
    _buttonsBarWidget->insertButton(DriveInfoClient::SynthesisStackedWidgetActivity, activityButton);

    // Stacked widget
    _stackedWidget = new QStackedWidget(this);
    mainVBox->addWidget(_stackedWidget);
    mainVBox->setStretchFactor(_stackedWidget, 1);

    setSynchronizedDefaultPage(&_defaultSynchronizedPageWidget, this);
    _stackedWidget->insertWidget(DriveInfoClient::SynthesisStackedWidgetSynchronized, _defaultSynchronizedPageWidget);

    _notImplementedLabel = new QLabel(this);
    _notImplementedLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _notImplementedLabel->setObjectName("defaultTitleLabel");
    _stackedWidget->insertWidget(DriveInfoClient::SynthesisStackedWidgetFavorites, _notImplementedLabel);

    _notImplementedLabel2 = new QLabel(this);
    _notImplementedLabel2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _notImplementedLabel2->setObjectName("defaultTitleLabel");
    _stackedWidget->insertWidget(DriveInfoClient::SynthesisStackedWidgetActivity, _notImplementedLabel2);

    // Bottom
    BottomWidget *bottomWidget = new BottomWidget(this);
    mainVBox->addWidget(bottomWidget);

    //// Locked app
    QVBoxLayout *lockedAppVesrionVBox = new QVBoxLayout(_lockedAppVesrionWidget);
    lockedAppVesrionVBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    QLabel *updateIconLabel = new QLabel(this);
    updateIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/pictures/kdrive-update.svg")
                                   .pixmap(lockedWindowSize.height() / 3, lockedWindowSize.height() / 3));
    updateIconLabel->setAlignment(Qt::AlignHCenter);
    lockedAppVesrionVBox->addWidget(updateIconLabel);

    lockedAppVesrionVBox->addSpacing(defaultPageSpacing);

    QLabel *lockedAppupdateAppLabel = new QLabel(tr("Update kDrive App"), this);
    lockedAppupdateAppLabel->setObjectName("defaultTitleLabel");
    lockedAppupdateAppLabel->setAlignment(Qt::AlignHCenter);
    lockedAppVesrionVBox->addWidget(lockedAppupdateAppLabel);

    lockedAppVesrionVBox->addSpacing(defaultPageSpacing);

    QLabel *lockedAppLabel = new QLabel(
        tr("This kDrive app version is not supported anymore. To access the latest features and enhancements, please update."),
        this);
    lockedAppLabel->setObjectName("defaultTextLabel");
    lockedAppLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    lockedAppLabel->setAlignment(Qt::AlignHCenter);
    lockedAppLabel->setWordWrap(true);
    lockedAppVesrionVBox->addWidget(lockedAppLabel);

    lockedAppVesrionVBox->addSpacing(defaultPageSpacing);

    QHBoxLayout *lockedAppUpdateButtonHBox = new QHBoxLayout();
    lockedAppUpdateButtonHBox->setAlignment(Qt::AlignHCenter);

    _lockedAppUpdateButton = new QPushButton();
    _lockedAppUpdateButton->setObjectName("KDCdefaultbutton");
    _lockedAppUpdateButton->setFlat(true);
    _lockedAppUpdateButton->setText(tr("Update"));
    _lockedAppUpdateButton->setEnabled(false);
    lockedAppUpdateButtonHBox->addWidget(_lockedAppUpdateButton);
    lockedAppVesrionVBox->addLayout(lockedAppUpdateButtonHBox);

    // TODO remove this button
    QPushButton *lockedAppCancelButton = new QPushButton();
    lockedAppCancelButton->setText(tr("Cancel"));
    lockedAppUpdateButtonHBox->addWidget(lockedAppCancelButton);
    connect(lockedAppCancelButton, &QPushButton::clicked, this, [this]() { onAppVersionLocked(_mainWidget->isVisible()); });

    // Shadow
    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    effect->setColor(KDC::GuiUtility::getShadowColor(true));
    setGraphicsEffect(effect);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    connect(languageFilter, &LanguageChangeFilter::retranslate, this, &SynthesisPopover::retranslateUi);

    connect(_errorsButton, &CustomToolButton::clicked, this, &SynthesisPopover::onOpenErrorsMenu);
    connect(_infosButton, &CustomToolButton::clicked, this, &SynthesisPopover::onOpenErrorsMenu);
    connect(_menuButton, &CustomToolButton::clicked, this, &SynthesisPopover::onOpenMiscellaneousMenu);
    connect(_driveSelectionWidget, &DriveSelectionWidget::driveSelected, this, &SynthesisPopover::onDriveSelected);
    connect(_driveSelectionWidget, &DriveSelectionWidget::addDrive, this, &SynthesisPopover::onAddDrive);
    connect(_statusBarWidget, &StatusBarWidget::pauseSync, this, &SynthesisPopover::onPauseSync);
    connect(_statusBarWidget, &StatusBarWidget::resumeSync, this, &SynthesisPopover::onResumeSync);
    connect(_statusBarWidget, &StatusBarWidget::linkActivated, this, &SynthesisPopover::onLinkActivated);
    connect(_buttonsBarWidget, &ButtonsBarWidget::buttonToggled, this, &SynthesisPopover::onButtonBarToggled);
    connect(_lockedAppUpdateButton, &QPushButton::clicked, this, &SynthesisPopover::onUpdateApp, Qt::UniqueConnection);
    connect(_lockedAppUpdateButton, &QPushButton::clicked, qApp, &QApplication::quit, Qt::UniqueConnection);
}

QUrl SynthesisPopover::syncUrl(int syncDbId, const QString &filePath) {
    QString fullFilePath = _gui->folderPath(syncDbId, filePath);
    return KDC::GuiUtility::getUrlFromLocalPath(fullFilePath);
}

void SynthesisPopover::openUrl(int syncDbId, const QString &filePath) {
    QUrl url = syncUrl(syncDbId, filePath);
    if (url.isValid()) {
        if (!QDesktopServices::openUrl(url)) {
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder url %1.").arg(url.toString()),
                                    QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void SynthesisPopover::getFirstSyncWithStatus(SyncStatus status, int driveDbId, int &syncDbId, bool &found) {
    found = false;
    for (auto const &syncInfoMapElt : _gui->syncInfoMap()) {
        if (syncInfoMapElt.second.status() == status && syncInfoMapElt.second.driveDbId() == driveDbId) {
            syncDbId = syncInfoMapElt.first;
            found = true;
            break;
        }
    }
}

void SynthesisPopover::getFirstSyncByPriority(int driveDbId, int &syncDbId, bool &found) {
    static QVector<SyncStatus> statusPriority = QVector<SyncStatus>()
                                                << SyncStatus::SyncStatusStarting << SyncStatus::SyncStatusRunning
                                                << SyncStatus::SyncStatusPauseAsked << SyncStatus::SyncStatusPaused
                                                << SyncStatus::SyncStatusStopAsked << SyncStatus::SyncStatusStoped
                                                << SyncStatus::SyncStatusError << SyncStatus::SyncStatusIdle;

    found = false;
    for (SyncStatus status : qAsConst(statusPriority)) {
        getFirstSyncWithStatus(status, driveDbId, syncDbId, found);
        if (found) {
            break;
        }
    }
    if (!found) {
        if (!_gui->syncInfoMap().empty()) {
            syncDbId = _gui->syncInfoMap().begin()->first;
            found = true;
        }
    }
}

void SynthesisPopover::refreshStatusBar(const DriveInfoClient &driveInfo) {
    static QVector<SyncStatus> statusPriority =
        QVector<SyncStatus>() << SyncStatusError << SyncStatusRunning << SyncStatusPauseAsked << SyncStatusPaused
                              << SyncStatusStopAsked << SyncStatusStoped << SyncStatusStarting << SyncStatusIdle;

    static QVector<SyncStep> syncStepPriority =
        QVector<SyncStep>() << SyncStepPropagation2 << SyncStepPropagation1 << SyncStepReconciliation4 << SyncStepReconciliation3
                            << SyncStepReconciliation2 << SyncStepReconciliation1 << SyncStepUpdateDetection2
                            << SyncStepUpdateDetection1 << SyncStepIdle << SyncStepDone << SyncStepNone;

    const auto &accountInfoMapIt = _gui->accountInfoMap().find(driveInfo.accountDbId());
    if (accountInfoMapIt == _gui->accountInfoMap().end()) {
        qCWarning(lcSynthesisPopover()) << "Account not found in accountInfoMap for accountDbId=" << driveInfo.accountDbId();
        return;
    }

    const auto userInfoMapIt = _gui->userInfoMap().find(accountInfoMapIt->second.userDbId());
    if (userInfoMapIt == _gui->userInfoMap().end()) {
        qCWarning(lcSynthesisPopover()) << "User not found in userInfoMap for userDbId=" << accountInfoMapIt->second.userDbId();
        return;
    }

    KDC::GuiUtility::StatusInfo statusInfo;
    if (userInfoMapIt->second.connected()) {
        statusInfo._status = SyncStatusIdle;
        int syncsInPropagationStep = 0;
        for (const auto &sync : _gui->syncInfoMap()) {
            const auto &syncInfo = sync.second;
            if (syncInfo.driveDbId() != driveInfo.dbId()) continue;

            statusInfo._syncedFiles += syncInfo.currentFile();
            statusInfo._totalFiles += syncInfo.totalFiles();
            if (statusInfo._estimatedRemainingTime < syncInfo.estimatedRemainingTime()) {
                statusInfo._estimatedRemainingTime = syncInfo.estimatedRemainingTime();
            }
            if (syncInfo.unresolvedConflicts()) {
                statusInfo._unresolvedConflicts = true;
            }
            if (statusPriority.indexOf(statusInfo._status) > statusPriority.indexOf(syncInfo.status())) {
                statusInfo._status = syncInfo.status();
            }
            if (syncStepPriority.indexOf(statusInfo._syncStep) > syncStepPriority.indexOf(syncInfo.step())) {
                statusInfo._syncStep = syncInfo.step();
            }
            if (syncInfo.step() != SyncStepPropagation2) {
                syncsInPropagationStep++;
            }
            if (syncInfo.virtualFileMode() != VirtualFileModeOff) {
                statusInfo._liteSyncActivated = true;
            }
        }

        statusInfo._oneSyncInPropagationStep = syncsInPropagationStep == 1;
    } else {
        statusInfo._disconnected = true;
    }

    _statusBarWidget->setStatus(statusInfo);
}

void SynthesisPopover::refreshStatusBar(std::map<int, DriveInfoClient>::const_iterator driveInfoMapIt) {
    _statusBarWidget->setCurrentDrive(driveInfoMapIt->first);
    _statusBarWidget->setSeveralSyncs(_gui->syncInfoMap().size() > 1);

    refreshStatusBar(driveInfoMapIt->second);
}

void SynthesisPopover::refreshStatusBar(int driveDbId) {
    refreshStatusBar(_gui->driveInfoMap().find(driveDbId));
}

void SynthesisPopover::refreshErrorsButton() {
    bool drivesWithErrors = false;
    bool drivesWithInfos = false;

    for (auto &[driveId, driveInfo] : _gui->driveInfoMap()) {
        std::map<int, SyncInfoClient> syncInfoMap;
        _gui->loadSyncInfoMap(driveId, syncInfoMap);
        if (syncInfoMap.empty()) {
            driveInfo.setUnresolvedErrorsCount(0);
            driveInfo.setAutoresolvedErrorsCount(0);
        }

        drivesWithErrors = drivesWithErrors || _gui->driveErrorsCount(driveId, true) > 0;
        drivesWithInfos = drivesWithInfos || _gui->driveErrorsCount(driveId, false) > 0;
    }

    _errorsButton->setVisible(_gui->hasGeneralErrors() || drivesWithErrors);
    if (_gui->hasGeneralErrors() || drivesWithErrors) {
        _infosButton->setVisible(false);
    } else {
        _infosButton->setVisible(drivesWithInfos);
    }
}

void SynthesisPopover::setSynchronizedDefaultPage(QWidget **widget, QWidget *parent) {
    if (!*widget) {
        *widget = new QWidget(parent);

        QVBoxLayout *vboxLayout = new QVBoxLayout();
        vboxLayout->setSpacing(defaultPageSpacing);
        vboxLayout->addStretch();

        QLabel *iconLabel = new QLabel(parent);
        iconLabel->setAlignment(Qt::AlignHCenter);
        iconLabel->setPixmap(QIcon(":/client/resources/icons/document types/file-default.svg")
                                 .pixmap(QSize(defaultLogoIconSize, defaultLogoIconSize)));
        vboxLayout->addWidget(iconLabel);

        _defaultTitleLabel = new QLabel(parent);
        _defaultTitleLabel->setObjectName("defaultTitleLabel");
        _defaultTitleLabel->setAlignment(Qt::AlignHCenter);
        vboxLayout->addWidget(_defaultTitleLabel);

        _defaultTextLabel = new QLabel(parent);
        _defaultTextLabel->setObjectName("defaultTextLabel");
        _defaultTextLabel->setAlignment(Qt::AlignHCenter);
        _defaultTextLabel->setWordWrap(true);
        _defaultTextLabel->setContextMenuPolicy(Qt::PreventContextMenu);

        vboxLayout->addWidget(_defaultTextLabel);

        vboxLayout->addStretch();
        (*widget)->setLayout(vboxLayout);

        connect(_defaultTextLabel, &QLabel::linkActivated, this, &SynthesisPopover::onLinkActivated);
    }

    // Set text
    if (_defaultTextLabel) {
        if (_gui->currentDriveDbId() != 0) {
            const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
            if (driveInfoIt == _gui->driveInfoMap().end()) {
                qCWarning(lcSynthesisPopover()) << "Drive not found in drive map for driveDbId=" << _gui->currentDriveDbId();
                return;
            }

            if (_gui->syncInfoMap().empty()) {
                // No folder
                _defaultTextLabelType = defaultTextLabelTypeNoSyncFolder;
            } else {
                // select a sync corresponding to the selected drive
                // TODO: this view can't represented sync how to open the right sync if we have only the current drive
                int syncDbId = _gui->syncInfoMap().begin()->first;
                for (const auto &sync : _gui->syncInfoMap()) {
                    if (sync.second.driveDbId() == _gui->currentDriveDbId()) {
                        syncDbId = sync.first;
                        break;
                    }
                }
                _localFolderUrl = syncUrl(syncDbId, QString());

                QString driveLink;
                _gui->getWebviewDriveLink(_gui->currentDriveDbId(), driveLink);
                if (driveLink.isEmpty()) {
                    qCWarning(lcSynthesisPopover) << "Error in onOpenWebviewDrive " << _gui->currentDriveDbId();
                    return;
                }

                _remoteFolderUrl = QUrl(driveLink);
                _defaultTextLabelType = defaultTextLabelTypeCanSync;
            }
        } else {
            // No account
            _defaultTextLabelType = defaultTextLabelTypeNoDrive;
        }
    } else {
        qCDebug(lcSynthesisPopover) << "Null pointer!";
        Q_ASSERT(false);
    }
}

void SynthesisPopover::handleRemovedDrives() {
    std::map<int, SyncInfoClient> syncInfoMap;
    _gui->loadSyncInfoMap(_gui->currentDriveDbId(), syncInfoMap);

    if (syncInfoMap.empty()) _statusBarWidget->reset();

    for (int widgetIndex = DriveInfoClient::SynthesisStackedWidgetFirstAdded; widgetIndex < _stackedWidget->count();) {
        QWidget *widget = _stackedWidget->widget(widgetIndex);
        bool driveIsFound = false;
        for (auto &[driveId, driveInfo] : _gui->driveInfoMap()) {
            if (driveInfo.synchronizedListWidget() == widget) {
                driveIsFound = true;
                ++widgetIndex;
                break;
            }
        }
        if (!driveIsFound) _stackedWidget->removeWidget(widget);
    }

    _driveSelectionWidget->selectDrive(_gui->currentDriveDbId());
    _statusBarWidget->setSeveralSyncs(syncInfoMap.size() > 1);
}

void SynthesisPopover::onConfigRefreshed() {
#ifdef CONSOLE_DEBUG
    std::cout << QTime::currentTime().toString("hh:mm:ss").toStdString() << " - SynthesisPopover::onUserListRefreshed"
              << std::endl;
#endif

    if (_gui->driveInfoMap().empty()) {
        reset();
    } else
        handleRemovedDrives();

    setSynchronizedDefaultPage(&_defaultSynchronizedPageWidget, this);
    refreshErrorsButton();
    forceRedraw();
}

void SynthesisPopover::onUpdateProgress(int syncDbId) {
    const auto &syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
    if (syncInfoMapIt != _gui->syncInfoMap().end()) {
        if (syncInfoMapIt->second.driveDbId() == _gui->currentDriveDbId()) {
            refreshStatusBar(_gui->currentDriveDbId());
        }
    }
}

void SynthesisPopover::onDriveQuotaUpdated(int driveDbId) {
#ifdef CONSOLE_DEBUG
    std::cout << QTime::currentTime().toString("hh:mm:ss").toStdString()
              << " - SynthesisPopover::onUpdateQuota account: " << driveDbId.toStdString() << std::endl;
#endif

    if (driveDbId == _gui->currentDriveDbId()) {
        const auto &driveInfoMapIt = _gui->driveInfoMap().find(driveDbId);
        if (driveInfoMapIt != _gui->driveInfoMap().end()) {
            _progressBarWidget->setUsedSize(driveInfoMapIt->second.totalSize(), driveInfoMapIt->second.used());
        }
    }
}

void SynthesisPopover::onRefreshStatusNeeded() {}

void SynthesisPopover::onItemCompleted(int syncDbId, const SyncFileItemInfo &itemInfo) {
#ifdef CONSOLE_DEBUG
    std::cout << QTime::currentTime().toString("hh:mm:ss").toStdString() << " - SynthesisPopover::onItemCompleted" << std::endl;
#endif

    const auto &syncInfoMapIt = _gui->syncInfoMap().find(syncDbId);
    if (syncInfoMapIt == _gui->syncInfoMap().end()) {
        qCDebug(lcSynthesisPopover()) << "Sync not found in sync map for syncDbId=" << syncDbId;
        return;
    }

    int driveDbId = syncInfoMapIt->second.driveDbId();

    auto driveInfoIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcSynthesisPopover()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        return;
    }

    if (itemInfo.status() == SyncFileStatusUnknown || itemInfo.status() == SyncFileStatusError ||
        itemInfo.status() == SyncFileStatusIgnored) {
        return;
    }

    if (!driveInfoIt->second.synchronizedListWidget()) {
        driveInfoIt->second.setSynchronizedListWidget(new QListWidget(this));
        driveInfoIt->second.synchronizedListWidget()->setSpacing(0);
        driveInfoIt->second.synchronizedListWidget()->setSelectionMode(QAbstractItemView::NoSelection);
        driveInfoIt->second.synchronizedListWidget()->setSelectionRectVisible(false);
        driveInfoIt->second.synchronizedListWidget()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        driveInfoIt->second.synchronizedListWidget()->setUniformItemSizes(true);
        driveInfoIt->second.setSynchronizedListStackPosition(
            _stackedWidget->addWidget(driveInfoIt->second.synchronizedListWidget()));
        if (_gui->currentDriveDbId() == driveInfoIt->first &&
            _buttonsBarWidget->position() == DriveInfoClient::SynthesisStackedWidgetSynchronized) {
            _stackedWidget->setCurrentIndex(driveInfoIt->second.synchronizedListStackPosition());
        }
    }

    // Add item to synchronized list
    SynchronizedItem synchronizedItem(syncDbId, itemInfo.path().isEmpty() ? itemInfo.newPath() : itemInfo.path(),
                                      itemInfo.remoteNodeId(), itemInfo.status(), itemInfo.direction(), itemInfo.type(),
                                      _gui->folderPath(syncDbId, itemInfo.path()), QDateTime::currentDateTime());

    driveInfoIt->second.synchronizedItemList().prepend(std::move(synchronizedItem));

    if (driveInfoIt->second.synchronizedItemList().count() > maxSynchronizedItems) {
        driveInfoIt->second.synchronizedItemList().removeLast();
    }

    if (this->isVisible() && driveDbId == _gui->currentDriveDbId()) {
        // Update at most each 500 ms
        if (QDateTime::currentMSecsSinceEpoch() - _lastRefresh > UPDATE_PROGRESS_DELAY) {
            _lastRefresh = QDateTime::currentMSecsSinceEpoch();
            for (int row = driveInfoIt->second.synchronizedItemList().count() - 1; row >= 0; row--) {
                if (!driveInfoIt->second.synchronizedItemList()[row].displayed()) {
                    addSynchronizedListWidgetItem(driveInfoIt->second, row);
                    driveInfoIt->second.synchronizedItemList()[row].setDisplayed(true);
                }
            }
        }
    }

    if (driveInfoIt->second.synchronizedListWidget()->count() > maxSynchronizedItems) {
        QListWidgetItem *item = driveInfoIt->second.synchronizedListWidget()->item(maxSynchronizedItems);
        SynchronizedItemWidget *widget =
            qobject_cast<SynchronizedItemWidget *>(driveInfoIt->second.synchronizedListWidget()->itemWidget(item));
        widget->stopTimer();
        widget->deleteLater();
        delete driveInfoIt->second.synchronizedListWidget()->takeItem(maxSynchronizedItems);
    }
}

void SynthesisPopover::onOpenErrorsMenu(bool checked) {
    Q_UNUSED(checked)

    QList<ErrorsPopup::DriveError> driveErrorList;
    getDriveErrorList(driveErrorList);

    if (driveErrorList.size() > 0 || _gui->generalErrorsCount() > 0) {
        CustomToolButton *button = qobject_cast<CustomToolButton *>(sender());
        QPoint position = QWidget::mapToGlobal(button->geometry().center());
        ErrorsPopup *errorsPopup = new ErrorsPopup(driveErrorList, _gui->generalErrorsCount(), position, this);
        connect(errorsPopup, &ErrorsPopup::accountSelected, this, &SynthesisPopover::onDisplayErrors);
        errorsPopup->show();
        errorsPopup->setModal(true);
    }
}

void SynthesisPopover::onDisplayErrors(int driveDbId) {
    displayErrors(driveDbId);
}

void SynthesisPopover::displayErrors(int driveDbId) {
    emit showParametersDialog(driveDbId, true);
}

void SynthesisPopover::reset() {
    _errorsButton->setVisible(false);
    _infosButton->setVisible(false);
    _driveSelectionWidget->clear();
    _progressBarWidget->reset();
    _statusBarWidget->reset();
    _stackedWidget->setCurrentIndex(DriveInfoClient::SynthesisStackedWidgetSynchronized);
}

void SynthesisPopover::addSynchronizedListWidgetItem(DriveInfoClient &driveInfoClient, int row) {
    if (row >= driveInfoClient.synchronizedItemList().count()) {
        return;
    }


    SynchronizedItemWidget *widget =
        new SynchronizedItemWidget(driveInfoClient.synchronizedItemList()[row], driveInfoClient.synchronizedListWidget());

    QListWidgetItem *widgetItem = new QListWidgetItem();
    driveInfoClient.synchronizedListWidget()->insertItem(row, widgetItem);
    driveInfoClient.synchronizedListWidget()->setItemWidget(widgetItem, widget);

    connect(widget, &SynchronizedItemWidget::openFolder, this, &SynthesisPopover::onOpenFolderItem);
    connect(widget, &SynchronizedItemWidget::open, this, &SynthesisPopover::onOpenItem);
    connect(widget, &SynchronizedItemWidget::addToFavourites, this, &SynthesisPopover::onAddToFavouriteItem);
    // connect(widget, &SynchronizedItemWidget::manageRightAndSharing, this, &SynthesisPopover::onManageRightAndSharingItem);
    connect(widget, &SynchronizedItemWidget::copyLink, this, &SynthesisPopover::onCopyLinkItem);
    connect(widget, &SynchronizedItemWidget::displayOnWebview, this, &SynthesisPopover::onOpenWebviewItem);
    connect(widget, &SynchronizedItemWidget::selectionChanged, this, &SynthesisPopover::onSelectionChanged);
    connect(this, &SynthesisPopover::cannotSelect, widget, &SynchronizedItemWidget::onCannotSelect);
}

void SynthesisPopover::getDriveErrorList(QList<ErrorsPopup::DriveError> &list) {
    list.clear();
    for (auto const &driveInfoElt : _gui->driveInfoMap()) {
        int driveUnresolvedErrorsCount = _gui->driveErrorsCount(driveInfoElt.first, true);
        int driveAutoresolvedErrorsCount = _gui->driveErrorsCount(driveInfoElt.first, false);
        if (driveUnresolvedErrorsCount > 0 || driveAutoresolvedErrorsCount > 0) {
            ErrorsPopup::DriveError driveError;
            driveError.driveDbId = driveInfoElt.first;
            driveError.driveName = driveInfoElt.second.name();
            driveError.unresolvedErrorsCount = driveUnresolvedErrorsCount;
            driveError.autoresolvedErrorsCount = driveAutoresolvedErrorsCount;
            list << driveError;
        }
    }
}

void SynthesisPopover::onUpdateSynchronizedListWidget() {
    if (_gui->currentDriveDbId() == 0) {
        return;
    }

    const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcSynthesisPopover()) << "Drive not found in drive map!";
        return;
    }

    if (!driveInfoIt->second.synchronizedListWidget()) {
        return;
    }

    driveInfoIt->second.synchronizedListWidget()->clear();

    for (int row = 0; row < driveInfoIt->second.synchronizedItemList().count(); row++) {
        addSynchronizedListWidgetItem(driveInfoIt->second, row);
        driveInfoIt->second.synchronizedItemList()[row].setDisplayed(true);
    }
}

void SynthesisPopover::onUpdateApp() {
    try {
        UpdaterClient::instance()->startInstaller();
    } catch (std::exception const &) {
        // Do nothing
    }
}

void SynthesisPopover::onUpdateAvailabalityChange() {
    if (!_lockedAppUpdateButton) return;

    QString statusString;
    UpdateState updateState = UpdateState::Error;
    try {
        statusString = UpdaterClient::instance()->statusString();
        updateState = UpdaterClient::instance()->updateState();
    } catch (std::exception const &) {
        return;
    }

    _lockedAppUpdateButton->setEnabled(updateState == UpdateState::Ready);
    switch (updateState) {
        case UpdateState::Error:
            _lockedAppUpdateButton->setText(tr("Unavailable"));
            break;
        case UpdateState::Ready:
            _lockedAppUpdateButton->setText(tr("Update"));
            break;
        case UpdateState::Downloading:
            _lockedAppUpdateButton->setText(tr("Update download in progress"));
            break;
        case UpdateState::Skipped:
            UpdaterClient::instance()->unskipUpdate();
        case UpdateState::Checking:
            _lockedAppUpdateButton->setText(tr("Looking for update..."));
            break;
        case UpdateState::ManualOnly:
            _lockedAppUpdateButton->setText(tr("Manual update"));
            break;
        // Error & None (up to date), if the app is locked, we should have an update available.
        default:
            _lockedAppUpdateButton->setText(tr("Error"));
            break;
    }

    connect(_lockedAppUpdateButton, &QPushButton::clicked, this, &SynthesisPopover::onStartInstaller, Qt::UniqueConnection);
}

void SynthesisPopover::onStartInstaller() {
    try {
        UpdaterClient::instance()->startInstaller();
    } catch (std::exception const &) {
        // Do nothing
    }
}

void SynthesisPopover::onAppVersionLocked(bool currentVersionLocked) {
    if (currentVersionLocked) {
        _mainWidget->hide();
        _lockedAppVesrionWidget->show();
        setFixedSize(lockedWindowSize);
        _gui->closeAllExcept(this);
        onUpdateAvailabalityChange();
    } else {
        _lockedAppVesrionWidget->hide();
        _mainWidget->show();
        setFixedSize(windowSize);
    }
    setPosition(_sysTrayIconRect);
}

void SynthesisPopover::onRefreshErrorList(int /*driveDbId*/) {
    refreshErrorsButton();
}

void SynthesisPopover::onOpenFolder(bool checked) {
    Q_UNUSED(checked)

    int syncDbId = qvariant_cast<int>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    openUrl(syncDbId);
}

void SynthesisPopover::onOpenWebview(bool checked) {
    Q_UNUSED(checked)

    if (_gui->currentDriveDbId() != 0) {
        const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
        if (driveInfoIt == _gui->driveInfoMap().end()) {
            qCWarning(lcSynthesisPopover()) << "Drive not found in drive map for driveDbId=" << _gui->currentDriveDbId();
            return;
        }

        QString driveLink;
        _gui->getWebviewDriveLink(_gui->currentDriveDbId(), driveLink);
        if (driveLink.isEmpty()) {
            qCWarning(lcSynthesisPopover) << "Error in onOpenWebviewDrive " << _gui->currentDriveDbId();
            return;
        }

        if (!QDesktopServices::openUrl(driveLink)) {
            qCWarning(lcSynthesisPopover) << "QDesktopServices::openUrl failed for " << driveLink;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to access web site %1.").arg(driveLink), QMessageBox::Ok,
                                    this);
            msgBox.exec();
        }
    }
}

void SynthesisPopover::onOpenMiscellaneousMenu(bool checked) {
    Q_UNUSED(checked)

    MenuWidget *menu = new MenuWidget(MenuWidget::Menu, this);

    // Open Folder
    std::map<int, SyncInfoClient> syncInfoMap;
    _gui->loadSyncInfoMap(_gui->currentDriveDbId(), syncInfoMap);
    if (syncInfoMap.size() >= 1) {
        QWidgetAction *foldersMenuAction = new QWidgetAction(this);
        MenuItemWidget *foldersMenuItemWidget = new MenuItemWidget(tr("Open in folder"));
        foldersMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/folder.svg");
        foldersMenuAction->setDefaultWidget(foldersMenuItemWidget);

        if (syncInfoMap.size() == 1) {
            auto const &syncInfoMapElt = syncInfoMap.begin();
            foldersMenuAction->setProperty(MenuWidget::actionTypeProperty.c_str(), syncInfoMapElt->first);
            connect(foldersMenuAction, &QWidgetAction::triggered, this, &SynthesisPopover::onOpenFolder);
        } else if (syncInfoMap.size() > 1) {
            foldersMenuItemWidget->setHasSubmenu(true);

            // Open folders submenu
            MenuWidget *submenu = new MenuWidget(MenuWidget::Submenu, menu);

            QActionGroup *openFolderActionGroup = new QActionGroup(this);
            openFolderActionGroup->setExclusive(true);

            QWidgetAction *openFolderAction;
            for (auto const &syncInfoMapElt : syncInfoMap) {
                openFolderAction = new QWidgetAction(this);
                openFolderAction->setProperty(MenuWidget::actionTypeProperty.c_str(), syncInfoMapElt.first);
                MenuItemWidget *openFolderMenuItemWidget = new MenuItemWidget(syncInfoMapElt.second.name());
                openFolderMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/folder.svg");
                openFolderAction->setDefaultWidget(openFolderMenuItemWidget);
                connect(openFolderAction, &QWidgetAction::triggered, this, &SynthesisPopover::onOpenFolder);
                openFolderActionGroup->addAction(openFolderAction);
            }

            submenu->addActions(openFolderActionGroup->actions());
            foldersMenuAction->setMenu(submenu);
        }

        menu->addAction(foldersMenuAction);
    }

    // Open web version
    QWidgetAction *driveOpenWebViewAction = new QWidgetAction(this);
    MenuItemWidget *driveOpenWebViewMenuItemWidget = new MenuItemWidget(tr("Open %1 web version").arg(APPLICATION_SHORTNAME));
    driveOpenWebViewMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/webview.svg");
    driveOpenWebViewAction->setDefaultWidget(driveOpenWebViewMenuItemWidget);
    driveOpenWebViewAction->setVisible(_gui->currentDriveDbId() != 0);
    connect(driveOpenWebViewAction, &QWidgetAction::triggered, this, &SynthesisPopover::onOpenWebview);
    menu->addAction(driveOpenWebViewAction);

    // Drive parameters
    if (_gui->driveInfoMap().size()) {
        QWidgetAction *driveParametersAction = new QWidgetAction(this);
        MenuItemWidget *driveParametersMenuItemWidget = new MenuItemWidget(tr("Drive parameters"));
        driveParametersMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/drive.svg");
        driveParametersAction->setDefaultWidget(driveParametersMenuItemWidget);
        connect(driveParametersAction, &QWidgetAction::triggered, this, &SynthesisPopover::onOpenDriveParameters);
        menu->addAction(driveParametersAction);
    }

    // Disable Notifications
    QWidgetAction *notificationsMenuAction = new QWidgetAction(this);
    bool notificationAlreadyDisabledForPeriod =
        _notificationsDisabled != NotificationsDisabledNever && _notificationsDisabled != NotificationsDisabledAlways;
    MenuItemWidget *notificationsMenuItemWidget =
        new MenuItemWidget(notificationAlreadyDisabledForPeriod
                               ? tr("Notifications disabled until %1").arg(_notificationsDisabledUntilDateTime.toString())
                               : tr("Disable Notifications"));
    notificationsMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/notification-off.svg");
    notificationsMenuItemWidget->setHasSubmenu(true);
    notificationsMenuAction->setDefaultWidget(notificationsMenuItemWidget);
    menu->addAction(notificationsMenuAction);

    // Disable Notifications submenu
    MenuWidget *submenu = new MenuWidget(MenuWidget::Submenu, menu);

    QActionGroup *notificationActionGroup = new QActionGroup(this);
    notificationActionGroup->setExclusive(true);

    const std::map<NotificationsDisabled, QString> &notificationMap =
        _notificationsDisabled == NotificationsDisabledNever || _notificationsDisabled == NotificationsDisabledAlways
            ? _notificationsDisabledMap
            : _notificationsDisabledForPeriodMap;

    QWidgetAction *notificationAction;
    for (auto const &notificationMapElt : notificationMap) {
        notificationAction = new QWidgetAction(this);
        notificationAction->setProperty(MenuWidget::actionTypeProperty.c_str(), notificationMapElt.first);
        QString text = QCoreApplication::translate("KDC::SynthesisPopover", notificationMapElt.second.toStdString().c_str());
        MenuItemWidget *notificationMenuItemWidget = new MenuItemWidget(text);
        notificationMenuItemWidget->setChecked(notificationMapElt.first == _notificationsDisabled);
        notificationAction->setDefaultWidget(notificationMenuItemWidget);
        connect(notificationAction, &QWidgetAction::triggered, this, &SynthesisPopover::onNotificationActionTriggered);
        notificationActionGroup->addAction(notificationAction);
    }

    submenu->addActions(notificationActionGroup->actions());
    notificationsMenuAction->setMenu(submenu);

    // Application preferences
    QWidgetAction *preferencesAction = new QWidgetAction(this);
    MenuItemWidget *preferencesMenuItemWidget = new MenuItemWidget(tr("Application preferences"));
    preferencesMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/parameters.svg");
    preferencesAction->setDefaultWidget(preferencesMenuItemWidget);
    connect(preferencesAction, &QWidgetAction::triggered, this, &SynthesisPopover::onOpenPreferences);
    menu->addAction(preferencesAction);

    // Help
    QWidgetAction *helpAction = new QWidgetAction(this);
    MenuItemWidget *helpMenuItemWidget = new MenuItemWidget(tr("Need help"));
    helpMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/help.svg");
    helpAction->setDefaultWidget(helpMenuItemWidget);
    connect(helpAction, &QWidgetAction::triggered, this, &SynthesisPopover::onDisplayHelp);
    menu->addAction(helpAction);

    // Quit
    QWidgetAction *exitAction = new QWidgetAction(this);
    MenuItemWidget *exitMenuItemWidget = new MenuItemWidget(tr("Quit kDrive"));
    exitMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/error-sync.svg");
    exitAction->setDefaultWidget(exitMenuItemWidget);
    connect(exitAction, &QWidgetAction::triggered, this, &SynthesisPopover::onExit);
    menu->addAction(exitAction);

    if (_debugMode) {
        // Test crash
        QWidgetAction *crashAction = new QWidgetAction(this);
        MenuItemWidget *crashMenuItemWidget = new MenuItemWidget("Test crash");
        crashAction->setDefaultWidget(crashMenuItemWidget);
        connect(crashAction, &QWidgetAction::triggered, this, &SynthesisPopover::onCrash);
        menu->addAction(crashAction);

        // Test crash enforce
        QWidgetAction *crashEnforceAction = new QWidgetAction(this);
        MenuItemWidget *crashEnforceMenuItemWidget = new MenuItemWidget("Test crash enforce");
        crashEnforceAction->setDefaultWidget(crashEnforceMenuItemWidget);
        connect(crashEnforceAction, &QWidgetAction::triggered, this, &SynthesisPopover::onCrashEnforce);
        menu->addAction(crashEnforceAction);

        // Test crash fatal
        QWidgetAction *crashFatalAction = new QWidgetAction(this);
        MenuItemWidget *crashFatalMenuItemWidget = new MenuItemWidget("Test crash fatal");
        crashFatalAction->setDefaultWidget(crashFatalMenuItemWidget);
        connect(crashFatalAction, &QWidgetAction::triggered, this, &SynthesisPopover::onCrashFatal);
        menu->addAction(crashFatalAction);
    }

    menu->exec(QWidget::mapToGlobal(_menuButton->geometry().center()));
}

void SynthesisPopover::onOpenPreferences(bool checked) {
    Q_UNUSED(checked)

    emit showParametersDialog();
}

void SynthesisPopover::onDisplayHelp(bool checked) {
    Q_UNUSED(checked)

    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void SynthesisPopover::onExit(bool checked) {
    Q_UNUSED(checked)

    hide();
    emit exit();
}

void SynthesisPopover::onCrash(bool checked) {
    Q_UNUSED(checked)

    emit crash();
}

void SynthesisPopover::onCrashEnforce(bool checked) {
    Q_UNUSED(checked)

    emit crashEnforce();
}

void SynthesisPopover::onCrashFatal(bool checked) {
    Q_UNUSED(checked)

    emit crashFatal();
}

void SynthesisPopover::onNotificationActionTriggered(bool checked) {
    Q_UNUSED(checked)

    bool notificationAlreadyDisabledForPeriod =
        _notificationsDisabled != NotificationsDisabledNever && _notificationsDisabled != NotificationsDisabledAlways;

    _notificationsDisabled = qvariant_cast<NotificationsDisabled>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    switch (_notificationsDisabled) {
        case NotificationsDisabledNever:
            _notificationsDisabledUntilDateTime = QDateTime();
            break;
        case NotificationsDisabledOneHour:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                      ? _notificationsDisabledUntilDateTime.addSecs(60 * 60)
                                                      : QDateTime::currentDateTime().addSecs(60 * 60);
            break;
        case NotificationsDisabledUntilTomorrow:
            _notificationsDisabledUntilDateTime = QDateTime(QDateTime::currentDateTime().addDays(1).date(), QTime(8, 0));
            break;
        case NotificationsDisabledTreeDays:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                      ? _notificationsDisabledUntilDateTime.addDays(3)
                                                      : QDateTime::currentDateTime().addDays(3);
            break;
        case NotificationsDisabledOneWeek:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                      ? _notificationsDisabledUntilDateTime.addDays(7)
                                                      : QDateTime::currentDateTime().addDays(7);
            break;
        case NotificationsDisabledAlways:
            _notificationsDisabledUntilDateTime = QDateTime();
            break;
    }

    emit disableNotifications(_notificationsDisabled, _notificationsDisabledUntilDateTime);
}

void SynthesisPopover::onOpenDriveParameters(bool checked) {
    Q_UNUSED(checked)

    emit showParametersDialog(_gui->currentDriveDbId());
}

void SynthesisPopover::onDriveSelected(int driveDbId) {
    const auto driveInfoIt = _gui->driveInfoMap().find(driveDbId);
    if (driveInfoIt == _gui->driveInfoMap().end()) {
        qCWarning(lcSynthesisPopover()) << "Drive not found in drive map for driveDbId=" << driveDbId;
        return;
    }

    bool newAccountSelected = (_gui->currentDriveDbId() != driveDbId);
    if (newAccountSelected) {
        _gui->setCurrentDriveDbId(driveDbId);
    }

    _progressBarWidget->setUsedSize(driveInfoIt->second.totalSize(), driveInfoIt->second.used());
    refreshStatusBar(driveInfoIt);
    setSynchronizedDefaultPage(&_defaultSynchronizedPageWidget, this);

    _buttonsBarWidget->selectButton(int(driveInfoIt->second.stackedWidget()));

    if (this->isVisible() && newAccountSelected) {
        emit updateItemList();
    }
}

void SynthesisPopover::onAddDrive() {
    emit addDrive();
}

void SynthesisPopover::onPauseSync(ActionTarget target, int syncDbId) {
    emit executeSyncAction(ActionTypeStop, target,
                           (target == ActionTargetSync    ? syncDbId
                            : target == ActionTargetDrive ? _gui->currentDriveDbId()
                                                          : 0));
}

void SynthesisPopover::onResumeSync(ActionTarget target, int syncDbId) {
    emit executeSyncAction(ActionTypeStart, target,
                           (target == ActionTargetSync    ? syncDbId
                            : target == ActionTargetDrive ? _gui->currentDriveDbId()
                                                          : 0));
}

void SynthesisPopover::onButtonBarToggled(int position) {
    const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
    if (driveInfoIt != _gui->driveInfoMap().end()) {
        driveInfoIt->second.setStackedWidget(DriveInfoClient::SynthesisStackedWidget(position));
    }

    switch (position) {
        case DriveInfoClient::SynthesisStackedWidgetSynchronized:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.synchronizedListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.synchronizedListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(DriveInfoClient::SynthesisStackedWidgetSynchronized);
            }
            break;
        case DriveInfoClient::SynthesisStackedWidgetFavorites:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.favoritesListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.favoritesListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(DriveInfoClient::SynthesisStackedWidgetFavorites);
            }
            break;
        case DriveInfoClient::SynthesisStackedWidgetActivity:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.activityListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.activityListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(DriveInfoClient::SynthesisStackedWidgetActivity);
            }
            break;
    }
}

void SynthesisPopover::onOpenFolderItem(const SynchronizedItem &item) {
    QString fullFilePath = _gui->folderPath(item.syncDbId(), item.filePath());
    // TODO : Add safety net file presence check, return if not found
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(fullFilePath)});
#elif defined(Q_OS_MACX)
    QProcess::execute("/usr/bin/osascript", {"-e", "tell application \"Finder\" to reveal POSIX file\"" + fullFilePath + "\""});
    QProcess::execute("/usr/bin/osascript", {"-e", "tell application \"Finder\" to activate"});
#else
    QStringList args;
    args << "--session";
    args << "--dest=org.freedesktop.FileManager1";
    args << "--type=method_call";
    args << "/org/freedesktop/FileManager1";
    args << "org.freedesktop.FileManager1.ShowItems";
    args << "array:string:file://" + fullFilePath;
    args << "string:";
    QProcess::execute("dbus-send", args);
#endif
}

void SynthesisPopover::onOpenItem(const SynchronizedItem &item) {
    openUrl(item.syncDbId(), item.filePath());
}

void SynthesisPopover::onAddToFavouriteItem(const SynchronizedItem &item) {
    Q_UNUSED(item)

    CustomMessageBox msgBox(QMessageBox::Information, tr("Not implemented!"), QMessageBox::Ok, this);
    msgBox.exec();
}

/*void SynthesisPopover::onManageRightAndSharingItem(const SynchronizedItem &item)
{
    _gui->onManageRightAndSharingItem(item.syncDbId(), item.filePath());
}*/

void SynthesisPopover::onCopyLinkItem(const SynchronizedItem &item) {
    _gui->onCopyLinkItem(_gui->currentDriveDbId(), item.fileId());
}

void SynthesisPopover::onOpenWebviewItem(const SynchronizedItem &item) {
    _gui->onOpenWebviewItem(_gui->currentDriveDbId(), item.fileId());
}

void SynthesisPopover::onSelectionChanged(bool isSelected) {
    emit cannotSelect(isSelected);
    forceRedraw();
}

void SynthesisPopover::onLinkActivated(const QString &link) {
    if (link == KDC::GuiUtility::learnMoreLink) {
        displayErrors(_gui->currentDriveDbId());
    } else if (link == KDC::GuiUtility::loginLink) {
        _gui->openLoginDialog(_gui->currentUserDbId(), true);
    } else {
        // URL link
        QUrl url = QUrl(link);
        if (url.isValid()) {
            if (!QDesktopServices::openUrl(url)) {
                qCWarning(lcSynthesisPopover) << "QDesktopServices::openUrl failed for " << link;
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(link), QMessageBox::Ok, this);
                msgBox.exec();
            }
        } else {
            qCWarning(lcSynthesisPopover) << "Invalid link " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Invalid link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void SynthesisPopover::retranslateUi() {
    _errorsButton->setToolTip(tr("Show errors and informations"));
    _infosButton->setToolTip(tr("Show informations"));
    _menuButton->setToolTip(tr("More actions"));
    _notImplementedLabel->setText(tr("Not implemented!"));

    if (_defaultTextLabel) {
        switch (_defaultTextLabelType) {
            case defaultTextLabelTypeNoSyncFolder:
                _defaultTextLabel->setText(tr("No synchronized folder for this Drive!"));
                break;
            case defaultTextLabelTypeNoDrive:
                _defaultTextLabel->setText(tr("No kDrive configured!"));
                break;
            case defaultTextLabelTypeCanSync:
                _defaultTextLabel->setText(
                    tr("You can synchronize files <a style=\"%1\" href=\"%2\">from your computer</a>"
                       " or on <a style=\"%1\" href=\"%3\">kdrive.infomaniak.com</a>.")
                        .arg(CommonUtility::linkStyle, _localFolderUrl.toString(), _remoteFolderUrl.toString()));
                break;
        }
    }
}

}  // namespace KDC
