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
#include "guirequests.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"
#include "libcommon/log/sentry/handler.h"

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
static const int trianglePosition = 100; // Position from side
static const int cornerRadius = 5;
static const int shadowBlurRadius = 20;
static const int driveBoxHMargin = 10;
static const int driveBoxVMargin = 10;
static const int defaultPageSpacing = 20;
static const int defaultLogoIconSize = 50;
static const int maxSynchronizedItems = 50;

Q_LOGGING_CATEGORY(lcSynthesisPopover, "gui.synthesispopover", QtInfoMsg)

SynthesisPopover::SynthesisPopover(std::shared_ptr<ClientGui> gui, bool debugCrash, QWidget *parent) :
    QDialog(parent),
    _gui(gui),
    _debugCrash(debugCrash) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setMinimumSize(windowSize);
    setMaximumSize(windowSize);

    initUI();

    (void) connect(this, &SynthesisPopover::updateItemList, this, &SynthesisPopover::onUpdateSynchronizedListWidget,
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
            popoverPosition = QPoint(screenRect.x() + screenRect.width() - rect().width() - triangleHeight,
                                     screenRect.y() + triangleHeight);
        } else if (position == KDC::GuiUtility::systrayPosition::Bottom) {
            popoverPosition = QPoint(screenRect.x() + screenRect.width() - rect().width() - triangleHeight,
                                     screenRect.y() + screenRect.height() - rect().height() - triangleHeight);
        } else if (position == KDC::GuiUtility::systrayPosition::Left) {
            popoverPosition = QPoint(screenRect.x() + triangleHeight,
                                     screenRect.y() + screenRect.height() - rect().height() - triangleHeight);
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
            popoverPosition = QPoint(screenRect.x(),
                                     trianglePositionTop ? _sysTrayIconRect.center().y() - trianglePosition
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
void SynthesisPopover::refreshLockedStatus() {
    auto updateState = UpdateState::Unknown;
    GuiRequests::updateState(updateState);
    onUpdateAvailabilityChange(updateState);
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
    Q_UNUSED(event)

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
            trianglePoint1 = QPoint(trianglePositionLeft
                                            ? trianglePosition - static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().width() - trianglePosition - static_cast<int>(round(triangleWidth / 2.0)),
                                    triangleHeight);
            trianglePoint2 = QPoint(trianglePositionLeft ? trianglePosition : rect().width() - trianglePosition, 0);
            trianglePoint3 = QPoint(trianglePositionLeft
                                            ? trianglePosition + static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().width() - trianglePosition + static_cast<int>(round(triangleWidth / 2.0)),
                                    triangleHeight);

            // Border
            painterPath.moveTo(trianglePoint3);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint1);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.arcTo(
                    QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)),
                    270, 90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Bottom) {
            // Triangle points
            trianglePoint1 = QPoint(trianglePositionLeft
                                            ? trianglePosition - static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().width() - trianglePosition - static_cast<int>(round(triangleWidth / 2.0)),
                                    rect().height() - triangleHeight);
            trianglePoint2 = QPoint(trianglePositionLeft ? trianglePosition : rect().width() - trianglePosition, rect().height());
            trianglePoint3 = QPoint(trianglePositionLeft
                                            ? trianglePosition + static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().width() - trianglePosition + static_cast<int>(round(triangleWidth / 2.0)),
                                    rect().height() - triangleHeight);

            // Border
            painterPath.moveTo(trianglePoint1);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint3);
            painterPath.arcTo(
                    QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)),
                    270, 90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Left) {
            // Triangle points
            trianglePoint1 = QPoint(triangleHeight,
                                    trianglePositionTop
                                            ? trianglePosition - static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().height() - trianglePosition - static_cast<int>(round(triangleWidth / 2.0)));
            trianglePoint2 = QPoint(0, trianglePositionTop ? trianglePosition : rect().height() - trianglePosition);
            trianglePoint3 = QPoint(triangleHeight,
                                    trianglePositionTop
                                            ? trianglePosition + static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().height() - trianglePosition + static_cast<int>(round(triangleWidth / 2.0)));

            // Border
            painterPath.moveTo(trianglePoint1);
            painterPath.lineTo(trianglePoint2);
            painterPath.lineTo(trianglePoint3);
            painterPath.arcTo(QRect(intRect.bottomLeft() - QPoint(0, cornerDiameter), QSize(cornerDiameter, cornerDiameter)), 180,
                              90);
            painterPath.arcTo(
                    QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)),
                    270, 90);
            painterPath.arcTo(QRect(intRect.topRight() - QPoint(cornerDiameter, 0), QSize(cornerDiameter, cornerDiameter)), 0,
                              90);
            painterPath.arcTo(QRect(intRect.topLeft(), QSize(cornerDiameter, cornerDiameter)), 90, 90);
            painterPath.closeSubpath();
        } else if (position == KDC::GuiUtility::systrayPosition::Right) {
            // Triangle
            trianglePoint1 = QPoint(rect().width() - triangleHeight,
                                    trianglePositionTop
                                            ? trianglePosition - static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().height() - trianglePosition - static_cast<int>(round(triangleWidth / 2.0)));
            trianglePoint2 = QPoint(rect().width(), trianglePositionTop ? trianglePosition : rect().height() - trianglePosition);
            trianglePoint3 = QPoint(rect().width() - triangleHeight,
                                    trianglePositionTop
                                            ? trianglePosition + static_cast<int>(round(triangleWidth / 2.0))
                                            : rect().height() - trianglePosition + static_cast<int>(round(triangleWidth / 2.0)));

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
                    QRect(intRect.bottomRight() - QPoint(cornerDiameter, cornerDiameter), QSize(cornerDiameter, cornerDiameter)),
                    270, 90);
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
    if (event->type() == QEvent::WindowDeactivate && !_synthesisBar->systemMove()) {
        setGraphicsEffect(nullptr);
        done(QDialog::Accepted);
    } else if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
        if (_gui->isConnected()) {
            // Activate/deactivate requests
            _gui->activateLoadInfo(event->type() == QEvent::Show);

            if (event->type() == QEvent::Show) {
                _synthesisBar->refreshErrorsButton();
                // Update the list of synchronized items
                qCDebug(lcSynthesisPopover) << "Show event";
                emit updateItemList();
                MatomoClient::sendVisit(MatomoNameField::PG_SynthesisPopover);
            }
        }
    }
    return ret;
}

void SynthesisPopover::initUI() {
    /*
     *  _mainWidget
     *    mainVBox
     *      _synthesisBar
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
     *  _lockedAppVersionWidget
     *    lockedAppVersionVBox
     *      updateIconLabel
     *      lockedAppUpdateAppLabel
     *      lockedAppLabel
     *      _lockedAppUpdateOptionalLabel (not on Linux with menu tray)
     *      lockedAppUpdateButton (not on Linux with menu tray)
     *
     */
    auto *appVBox = new QVBoxLayout();
    appVBox->setContentsMargins(0, 0, 0, 0);
    setLayout(appVBox);

    _mainWidget = new QWidget(this);
    _lockedAppVersionWidget = new QWidget(this);
    _lockedAppVersionWidget->hide();

    appVBox->addWidget(_mainWidget);
    appVBox->addWidget(_lockedAppVersionWidget);

    auto *mainVBox = new QVBoxLayout(_mainWidget);
    mainVBox->setContentsMargins(triangleHeight, triangleHeight, triangleHeight, triangleHeight);
    mainVBox->setSpacing(0);

    // Tool bar
    _synthesisBar = new SynthesisBar(_gui, _debugCrash);
    mainVBox->addWidget(_synthesisBar);

    // Drive selection
    auto *hBoxDriveBar = new QHBoxLayout();
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

    auto *synchronizedButton = new CustomTogglePushButton(tr("Synchronized"), _buttonsBarWidget);
    synchronizedButton->setIconPath(":/client/resources/icons/actions/sync.svg");
    _buttonsBarWidget->insertButton(toInt(DriveInfoClient::SynthesisStackedWidget::Synchronized), synchronizedButton);

    auto *favoritesButton = new CustomTogglePushButton(tr("Favorites"), _buttonsBarWidget);
    favoritesButton->setIconPath(":/client/resources/icons/actions/favorite.svg");
    _buttonsBarWidget->insertButton(toInt(DriveInfoClient::SynthesisStackedWidget::Favorites), favoritesButton);

    auto *activityButton = new CustomTogglePushButton(tr("Activity"), _buttonsBarWidget);
    activityButton->setIconPath(":/client/resources/icons/actions/notifications.svg");
    _buttonsBarWidget->insertButton(toInt(DriveInfoClient::SynthesisStackedWidget::Activity), activityButton);

    // Stacked widget
    _stackedWidget = new QStackedWidget(this);
    mainVBox->addWidget(_stackedWidget);
    mainVBox->setStretchFactor(_stackedWidget, 1);

    setSynchronizedDefaultPage(&_defaultSynchronizedPageWidget, this);
    _stackedWidget->insertWidget(toInt(DriveInfoClient::SynthesisStackedWidget::Synchronized), _defaultSynchronizedPageWidget);

    _notImplementedLabel = new QLabel(this);
    _notImplementedLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _notImplementedLabel->setObjectName("defaultTitleLabel");
    _stackedWidget->insertWidget(toInt(DriveInfoClient::SynthesisStackedWidget::Favorites), _notImplementedLabel);

    _notImplementedLabel2 = new QLabel(this);
    _notImplementedLabel2->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    _notImplementedLabel2->setObjectName("defaultTitleLabel");
    _stackedWidget->insertWidget(toInt(DriveInfoClient::SynthesisStackedWidget::Activity), _notImplementedLabel2);

    // Bottom
    auto *bottomWidget = new BottomWidget(this);
    mainVBox->addWidget(bottomWidget);

    //// Locked app
    auto *lockedAppVersionVBox = new QVBoxLayout(_lockedAppVersionWidget);
    lockedAppVersionVBox->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    lockedAppVersionVBox->setContentsMargins(40, 40, 40, 40);
    // Update icon
    auto *updateIconLabel = new QLabel(this);
    updateIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/pictures/kdrive-update.svg")
                                       .pixmap(lockedWindowSize.height() / 3, lockedWindowSize.height() / 3));
    updateIconLabel->setAlignment(Qt::AlignHCenter);
    lockedAppVersionVBox->addWidget(updateIconLabel);

    lockedAppVersionVBox->addSpacing(defaultPageSpacing);

    // Update app label
    _lockedAppupdateAppLabel = new QLabel(this);
    _lockedAppupdateAppLabel->setObjectName("defaultTitleLabel");
    _lockedAppupdateAppLabel->setAlignment(Qt::AlignHCenter);
    lockedAppVersionVBox->addWidget(_lockedAppupdateAppLabel);

    lockedAppVersionVBox->addSpacing(defaultPageSpacing);

    // Locked app label
    _lockedAppLabel = new QLabel(this);
    _lockedAppLabel->setObjectName("defaultTextLabel");
    _lockedAppLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    _lockedAppLabel->setAlignment(Qt::AlignHCenter);
    _lockedAppLabel->setWordWrap(true);
    lockedAppVersionVBox->addWidget(_lockedAppLabel);

    lockedAppVersionVBox->addSpacing(defaultPageSpacing);

    // Update button
    auto *lockedAppUpdateButtonHBox = new QHBoxLayout();
    lockedAppUpdateButtonHBox->setAlignment(Qt::AlignHCenter);

    _lockedAppUpdateButton = new QPushButton();
    _lockedAppUpdateButton->setObjectName("KDCdefaultbutton");
    _lockedAppUpdateButton->setFlat(true);
    _lockedAppUpdateButton->setEnabled(false);
    lockedAppUpdateButtonHBox->addWidget(_lockedAppUpdateButton);
    lockedAppVersionVBox->addLayout(lockedAppUpdateButtonHBox);

#ifdef Q_OS_LINUX
    // On Linux, the update button is not displayed, the update need to be done manually by the user (download on the website)
    _lockedAppUpdateButton->hide();
    _lockedAppUpdateManualLabel = new QLabel(this);
    _lockedAppUpdateManualLabel->setObjectName("defaultTextLabel");
    _lockedAppUpdateManualLabel->setAlignment(Qt::AlignHCenter);
    _lockedAppUpdateManualLabel->setWordWrap(true);
    _lockedAppUpdateManualLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    lockedAppVersionVBox->addWidget(_lockedAppUpdateManualLabel);
#endif

    // Shadow
    auto *effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(shadowBlurRadius);
    effect->setOffset(0);
    effect->setColor(KDC::GuiUtility::getShadowColor(true));
    setGraphicsEffect(effect);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    auto *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    (void) connect(languageFilter, &LanguageChangeFilter::retranslate, this, &SynthesisPopover::retranslateUi);

    (void) connect(_synthesisBar, &SynthesisBar::showParametersDialog, this, &SynthesisPopover::showParametersDialog);
    (void) connect(_synthesisBar, &SynthesisBar::disableNotifications, this, &SynthesisPopover::disableNotifications);
    (void) connect(_synthesisBar, &SynthesisBar::exit, this, &SynthesisPopover::exit);
    (void) connect(_synthesisBar, &SynthesisBar::crash, this, &SynthesisPopover::crash);
    (void) connect(_synthesisBar, &SynthesisBar::crashServer, this, &SynthesisPopover::crashServer);
    (void) connect(_synthesisBar, &SynthesisBar::crashEnforce, this, &SynthesisPopover::crashEnforce);
    (void) connect(_synthesisBar, &SynthesisBar::crashFatal, this, &SynthesisPopover::crashEnforce);
    (void) connect(_driveSelectionWidget, &DriveSelectionWidget::driveSelected, this, &SynthesisPopover::onDriveSelected);
    (void) connect(_driveSelectionWidget, &DriveSelectionWidget::addDrive, this, &SynthesisPopover::onAddDrive);
    (void) connect(_statusBarWidget, &StatusBarWidget::pauseSync, this, &SynthesisPopover::onPauseSync);
    (void) connect(_statusBarWidget, &StatusBarWidget::resumeSync, this, &SynthesisPopover::onResumeSync);
    (void) connect(_statusBarWidget, &StatusBarWidget::linkActivated, this, &SynthesisPopover::onLinkActivated);
    (void) connect(_buttonsBarWidget, &ButtonsBarWidget::buttonToggled, this, &SynthesisPopover::onButtonBarToggled);

    (void) connect(_lockedAppUpdateButton, &QPushButton::clicked, this, &SynthesisPopover::onStartInstaller,
                   Qt::UniqueConnection);
    (void) connect(_gui.get(), &ClientGui::updateStateChanged, this, &SynthesisPopover::onUpdateAvailabilityChange,
                   Qt::UniqueConnection);
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
    for (auto const &syncInfoMapElt: _gui->syncInfoMap()) {
        if (syncInfoMapElt.second.status() == status && syncInfoMapElt.second.driveDbId() == driveDbId) {
            syncDbId = syncInfoMapElt.first;
            found = true;
            break;
        }
    }
}

void SynthesisPopover::getFirstSyncByPriority(int driveDbId, int &syncDbId, bool &found) {
    static QVector<SyncStatus> statusPriority =
            QVector<SyncStatus>() << SyncStatus::Starting << SyncStatus::Running << SyncStatus::PauseAsked << SyncStatus::Paused
                                  << SyncStatus::StopAsked << SyncStatus::Stopped << SyncStatus::Error << SyncStatus::Idle;

    found = false;
    for (SyncStatus status: qAsConst(statusPriority)) {
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
            QVector<SyncStatus>() << SyncStatus::Error << SyncStatus::Running << SyncStatus::PauseAsked << SyncStatus::Paused
                                  << SyncStatus::StopAsked << SyncStatus::Stopped << SyncStatus::Starting << SyncStatus::Idle;

    static QVector<SyncStep> SyncStepPriority = QVector<SyncStep>() << SyncStep::Propagation2 << SyncStep::Propagation1
                                                                    << SyncStep::Reconciliation4 << SyncStep::Reconciliation3
                                                                    << SyncStep::Reconciliation2 << SyncStep::Reconciliation1
                                                                    << SyncStep::UpdateDetection2 << SyncStep::UpdateDetection1
                                                                    << SyncStep::Idle << SyncStep::Done << SyncStep::None;

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
        int syncsInPropagationStep = 0;
        int syncsProcessed = 0;
        for (const auto &sync: _gui->syncInfoMap()) {
            const auto &syncInfo = sync.second;
            if (syncInfo.driveDbId() != driveInfo.dbId()) continue;

            statusInfo._syncedFiles += syncInfo.currentFile();
            statusInfo._totalFiles += syncInfo.totalFiles();
            if (syncInfo.unresolvedConflicts()) {
                statusInfo._unresolvedConflicts = true;
            }
            if (syncInfo.step() != SyncStep::Propagation2) {
                syncsInPropagationStep++;
            }
            if (syncInfo.virtualFileMode() != VirtualFileMode::Off) {
                statusInfo._liteSyncActivated = true;
            }

            if (syncsProcessed++ == 0) {
                statusInfo._estimatedRemainingTime = syncInfo.estimatedRemainingTime();
                statusInfo._status = syncInfo.status();
                statusInfo._syncStep = syncInfo.step();
            } else {
                if (statusInfo._estimatedRemainingTime < syncInfo.estimatedRemainingTime()) {
                    statusInfo._estimatedRemainingTime = syncInfo.estimatedRemainingTime();
                }
                if (statusPriority.indexOf(statusInfo._status) > statusPriority.indexOf(syncInfo.status())) {
                    statusInfo._status = syncInfo.status();
                }
                if (SyncStepPriority.indexOf(statusInfo._syncStep) > SyncStepPriority.indexOf(syncInfo.step())) {
                    statusInfo._syncStep = syncInfo.step();
                }
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

        (void) connect(_defaultTextLabel, &QLabel::linkActivated, this, &SynthesisPopover::onLinkActivated);
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
                for (const auto &sync: _gui->syncInfoMap()) {
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

    for (int widgetIndex = toInt(DriveInfoClient::SynthesisStackedWidget::FirstAdded); widgetIndex < _stackedWidget->count();) {
        QWidget *widget = _stackedWidget->widget(widgetIndex);
        bool driveIsFound = false;
        for (auto &[driveId, driveInfo]: _gui->driveInfoMap()) {
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
    _synthesisBar->refreshErrorsButton();
    retranslateUi();
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

    if (itemInfo.status() == SyncFileStatus::Unknown || itemInfo.status() == SyncFileStatus::Error ||
        itemInfo.status() == SyncFileStatus::Ignored) {
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
            _buttonsBarWidget->position() == toInt(DriveInfoClient::SynthesisStackedWidget::Synchronized)) {
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
            for (int row = static_cast<int>(driveInfoIt->second.synchronizedItemList().count() - 1); row >= 0; row--) {
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

void SynthesisPopover::reset() {
    _synthesisBar->reset();
    _driveSelectionWidget->clear();
    _progressBarWidget->reset();
    _statusBarWidget->reset();
    _stackedWidget->setCurrentIndex(toInt(DriveInfoClient::SynthesisStackedWidget::Synchronized));
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

    (void) connect(widget, &SynchronizedItemWidget::openFolder, this, &SynthesisPopover::onOpenFolderItem);
    (void) connect(widget, &SynchronizedItemWidget::open, this, &SynthesisPopover::onOpenItem);
    (void) connect(widget, &SynchronizedItemWidget::addToFavourites, this, &SynthesisPopover::onAddToFavouriteItem);
    (void) connect(widget, &SynchronizedItemWidget::copyLink, this, &SynthesisPopover::onCopyLinkItem);
    (void) connect(widget, &SynchronizedItemWidget::displayOnWebview, this, &SynthesisPopover::onOpenWebviewItem);
    (void) connect(widget, &SynchronizedItemWidget::selectionChanged, this, &SynthesisPopover::onSelectionChanged);
    (void) connect(this, &SynthesisPopover::cannotSelect, widget, &SynchronizedItemWidget::onCannotSelect);
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

void SynthesisPopover::onUpdateAvailabilityChange(const UpdateState updateState) {
    if (!_lockedAppUpdateButton) return;
    if (_lockedAppVersionWidget->isHidden()) return;

    _lockedAppUpdateButton->setEnabled(updateState == UpdateState::Ready || updateState == UpdateState::Available);
    switch (updateState) {
        case UpdateState::Ready:
        case UpdateState::Available:
            _lockedAppUpdateButton->setText(tr("Update"));
            break;
        case UpdateState::Downloading:
            _lockedAppUpdateButton->setText(tr("Update download in progress"));
            break;
        case UpdateState::Checking:
            _lockedAppUpdateButton->setText(tr("Looking for update..."));
            break;
        case UpdateState::ManualUpdateAvailable:
            _lockedAppUpdateButton->setText(tr("Manual update"));
            break;
        default:
            _lockedAppUpdateButton->setText(tr("Unavailable"));
            sentry::Handler::captureMessage(sentry::Level::Fatal, "AppLocked",
                                            "HTTP Error426 received but unable to fetch an update");
            break;
    }
}

void SynthesisPopover::onStartInstaller() const noexcept {
    VersionInfo versionInfo;
    GuiRequests::versionInfo(versionInfo);
    _gui->onShowWindowsUpdateDialog(versionInfo);
}

void SynthesisPopover::onAppVersionLocked(bool currentVersionLocked) {
    if (currentVersionLocked && _lockedAppVersionWidget->isHidden()) {
        _mainWidget->hide();
        _lockedAppVersionWidget->show();
        setFixedSize(lockedWindowSize);
        _gui->closeAllExcept(this);
        refreshLockedStatus();
    } else if (!currentVersionLocked && _mainWidget->isHidden()) {
        _lockedAppVersionWidget->hide();
        _mainWidget->show();
        setFixedSize(windowSize);
    }
    setPosition(_sysTrayIconRect);
}

void SynthesisPopover::onRefreshErrorList(int /*driveDbId*/) {
    _synthesisBar->refreshErrorsButton();
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

    retranslateUi();
}

void SynthesisPopover::onAddDrive() {
    MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "addDriveButton");
    emit addDrive();
}

void SynthesisPopover::onPauseSync(ActionTarget target, int syncDbId) {
    const int driveToMatomo =
            (target == ActionTarget::AllDrives ? -1 : (target == ActionTarget::Drive ? _gui->currentDriveDbId() : syncDbId));
    MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "pauseSyncButton", driveToMatomo);

    emit executeSyncAction(ActionType::Stop, target,
                           (target == ActionTarget::Sync    ? syncDbId
                            : target == ActionTarget::Drive ? _gui->currentDriveDbId()
                                                            : 0));
}

void SynthesisPopover::onResumeSync(ActionTarget target, int syncDbId) {
    const int driveToMatomo =
            (target == ActionTarget::AllDrives ? -1 : (target == ActionTarget::Drive ? _gui->currentDriveDbId() : syncDbId));
    MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "resumeSyncButton", driveToMatomo);

    emit executeSyncAction(ActionType::Start, target,
                           (target == ActionTarget::Sync    ? syncDbId
                            : target == ActionTarget::Drive ? _gui->currentDriveDbId()
                                                            : 0));
}

void SynthesisPopover::onButtonBarToggled(int position) {
    const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
    if (driveInfoIt != _gui->driveInfoMap().end()) {
        driveInfoIt->second.setStackedWidget(DriveInfoClient::SynthesisStackedWidget(position));
    }

    switch (fromInt<DriveInfoClient::SynthesisStackedWidget>(position)) {
        case DriveInfoClient::SynthesisStackedWidget::Synchronized:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.synchronizedListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.synchronizedListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(toInt(DriveInfoClient::SynthesisStackedWidget::Synchronized));
            }
            break;
        case DriveInfoClient::SynthesisStackedWidget::Favorites:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.favoritesListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.favoritesListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(toInt(DriveInfoClient::SynthesisStackedWidget::Favorites));
            }
            break;
        case DriveInfoClient::SynthesisStackedWidget::Activity:
            if (driveInfoIt != _gui->driveInfoMap().end() && driveInfoIt->second.activityListStackPosition()) {
                _stackedWidget->setCurrentIndex(driveInfoIt->second.activityListStackPosition());
            } else {
                _stackedWidget->setCurrentIndex(toInt(DriveInfoClient::SynthesisStackedWidget::Activity));
            }
            break;
        default:
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
        _synthesisBar->displayErrors(_gui->currentDriveDbId());
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
            if (link == _localFolderUrl.toString()) {
                MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "localFolderLink");
            } else if (link == _remoteFolderUrl.toString()) {
                MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "remoteFolderLink");
            } else {
                MatomoClient::sendEvent("synthesisPopover", MatomoEventAction::Click, "unknownLink");
            }
        } else {
            qCWarning(lcSynthesisPopover) << "Invalid link " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Invalid link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void SynthesisPopover::retranslateUi() {
    _notImplementedLabel->setText(tr("Not implemented!"));
    _lockedAppupdateAppLabel->setText(tr("Update kDrive App"));
    _lockedAppLabel->setText(tr(
            "This kDrive app version is not supported anymore. To access the latest features and enhancements, please update."));
    _lockedAppUpdateButton->setText(tr("Update"));
#ifdef Q_OS_LINUX
    _lockedAppUpdateManualLabel->setText(tr("Please download the latest version on the website."));
#endif // Q_OS_LINUX

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

} // namespace KDC
