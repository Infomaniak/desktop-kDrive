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

#include "synthesisbar.h"
#include "errorspopup.h"
#include "languagechangefilter.h"
#include "config.h"
#include "libcommongui/matomoclient.h"

#include <QActionGroup>
#include <QBoxLayout>
#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QWindow>

namespace KDC {

static const int toolBarHMargin = 10;
static const int toolBarVMargin = 10;
static const int toolBarSpacing = 10;
static const int logoIconSize = 30;

Q_LOGGING_CATEGORY(lcSynthesisBar, "gui.synthesisbar", QtInfoMsg)

const std::map<NotificationsDisabled, QString> &SynthesisBar::notificationsDisabledMap() {
    static const std::map<NotificationsDisabled, QString> map = {
            {NotificationsDisabled::Never, tr("Never")},
            {NotificationsDisabled::OneHour, tr("During 1 hour")},
            {NotificationsDisabled::UntilTomorrow, tr("Until tomorrow 8:00AM")},
            {NotificationsDisabled::ThreeDays, tr("During 3 days")},
            {NotificationsDisabled::OneWeek, tr("During 1 week")},
            {NotificationsDisabled::Always, tr("Always")}};
    return map;
}

const std::map<NotificationsDisabled, QString> &SynthesisBar::notificationsDisabledForPeriodMap() {
    static const std::map<NotificationsDisabled, QString> map = {
            {NotificationsDisabled::Never, tr("Never")},
            {NotificationsDisabled::OneHour, tr("For 1 more hour")},
            {NotificationsDisabled::UntilTomorrow, tr("Until tomorrow 8:00AM")},
            {NotificationsDisabled::ThreeDays, tr("For 3 more days")},
            {NotificationsDisabled::OneWeek, tr("For 1 more week")},
            {NotificationsDisabled::Always, tr("Always")}};
    return map;
}

SynthesisBar::SynthesisBar(std::shared_ptr<ClientGui> gui, bool debugCrash, QWidget *parent) :
    QWidget(parent),
    _gui(gui),
    _debugCrash(debugCrash) {
    _notificationsDisabled = ParametersCache::instance()->parametersInfo().notificationsDisabled();

    QCoreApplication::instance()->installEventFilter(this);

    auto *hBox = new QHBoxLayout();
    setLayout(hBox);

    hBox->setContentsMargins(toolBarHMargin, toolBarVMargin, toolBarHMargin, toolBarVMargin);
    hBox->setSpacing(toolBarSpacing);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                 .pixmap(logoIconSize, logoIconSize));
    hBox->addWidget(iconLabel);

    hBox->addStretch();

    _errorsButton = new CustomToolButton(this);
    _errorsButton->setObjectName("errorsButton");
    _errorsButton->setIconPath(":/client/resources/icons/actions/warning.svg");
    _errorsButton->setVisible(false);
    _errorsButton->setWithMenu(true);
    hBox->addWidget(_errorsButton);

    _infosButton = new CustomToolButton(this);
    _infosButton->setObjectName("informationButton");
    _infosButton->setIconPath(":/client/resources/icons/actions/information.svg");
    _infosButton->setVisible(false);
    _infosButton->setWithMenu(true);
    hBox->addWidget(_infosButton);

    _menuButton = new CustomToolButton(this);
    _menuButton->setIconPath(":/client/resources/icons/actions/menu.svg");
    hBox->addWidget(_menuButton);

    // Init labels and setup connection for on the fly translation
    retranslateUi();
    auto *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);
    (void) connect(languageFilter, &LanguageChangeFilter::retranslate, this, &SynthesisBar::retranslateUi);

    (void) connect(_errorsButton, &CustomToolButton::clicked, this, &SynthesisBar::onOpenErrorsMenu);
    (void) connect(_infosButton, &CustomToolButton::clicked, this, &SynthesisBar::onOpenErrorsMenu);
    (void) connect(_menuButton, &CustomToolButton::clicked, this, &SynthesisBar::onOpenMiscellaneousMenu);
}

void SynthesisBar::refreshErrorsButton() {
    bool drivesWithErrors = false;
    bool drivesWithInfos = false;

    for (auto &[driveId, driveInfo]: _gui->driveInfoMap()) {
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

void SynthesisBar::reset() {
    _errorsButton->setVisible(false);
    _infosButton->setVisible(false);
}

QUrl SynthesisBar::syncUrl(int syncDbId, const QString &filePath) {
    const QString fullFilePath = _gui->folderPath(syncDbId, filePath);
    return KDC::GuiUtility::getUrlFromLocalPath(fullFilePath);
}

void SynthesisBar::getDriveErrorList(QList<ErrorsPopup::DriveError> &list) {
    list.clear();
    for (auto const &driveInfoElt: _gui->driveInfoMap()) {
        const int driveUnresolvedErrorsCount = _gui->driveErrorsCount(driveInfoElt.first, true);
        const int driveAutoresolvedErrorsCount = _gui->driveErrorsCount(driveInfoElt.first, false);
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

void SynthesisBar::displayErrors(int driveDbId) {
    emit showParametersDialog(driveDbId, true);
}

void SynthesisBar::openUrl(int syncDbId, const QString &filePath) {
    const QUrl url = syncUrl(syncDbId, filePath);
    if (url.isValid()) {
        if (!QDesktopServices::openUrl(url)) {
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder url %1.").arg(url.toString()),
                                    QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

bool SynthesisBar::event(QEvent *event) {
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate) {
        const QList<QToolButton *> buttonList = findChildren<QToolButton *>();
        for (QToolButton *button: buttonList) {
            button->setEnabled(event->type() == QEvent::WindowActivate ? true : false);
        }
    }
    return QWidget::event(event);
}

void SynthesisBar::showEvent(QShowEvent *event) {
    Q_UNUSED(event)

    retranslateUi();
}

void SynthesisBar::mousePressEvent(QMouseEvent *event) {
    if (allowMove() && event->buttons() == Qt::LeftButton) {
        _systemMove = true;
        (void) window()->windowHandle()->startSystemMove();
    }
    QWidget::mousePressEvent(event);
}

bool SynthesisBar::eventFilter(QObject *obj, QEvent *event) {
    Q_UNUSED(obj)

    // Can't use mouseReleaseEvent or mouseMoveEvent because of a Qt 6.2 bug
    if (allowMove() && event->type() == QEvent::MouseMove) {
        _systemMove = false;
    }
    return false;
}

void SynthesisBar::onDisplayErrors(int driveDbId) {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "displayErrors", driveDbId);
    displayErrors(driveDbId);
}

void SynthesisBar::onOpenErrorsMenu() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "openErrorsMenu");
    QList<ErrorsPopup::DriveError> driveErrorList;
    getDriveErrorList(driveErrorList);

    if (!driveErrorList.empty() || _gui->generalErrorsCount() > 0) {
        CustomToolButton *button = qobject_cast<CustomToolButton *>(sender());
        const QPoint position = QWidget::mapToGlobal(button->geometry().center());
        auto *errorsPopup = new ErrorsPopup(driveErrorList, _gui->generalErrorsCount(), position, this);
        (void) connect(errorsPopup, &ErrorsPopup::accountSelected, this, &SynthesisBar::onDisplayErrors);
        errorsPopup->show();
        errorsPopup->setModal(true);
    }
}

void SynthesisBar::onOpenMiscellaneousMenu() {
    auto *menu = new MenuWidget(MenuWidget::Menu, this);

    MatomoClient::sendVisit(MatomoNameField::PG_SynthesisPopover_KebabMenu);
    // Open Folder
    std::map<int, SyncInfoClient> syncInfoMap;
    _gui->loadSyncInfoMap(_gui->currentDriveDbId(), syncInfoMap);
    if (!syncInfoMap.empty()) {
        auto *foldersMenuAction = new QWidgetAction(this);
        auto *foldersMenuItemWidget = new MenuItemWidget(tr("Open in folder"));
        foldersMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/folder.svg");
        foldersMenuAction->setDefaultWidget(foldersMenuItemWidget);

        if (syncInfoMap.size() == 1) {
            auto const &syncInfoMapElt = syncInfoMap.begin();
            foldersMenuAction->setProperty(MenuWidget::actionTypeProperty.c_str(), syncInfoMapElt->first);
            (void) connect(foldersMenuAction, &QWidgetAction::triggered, this, &SynthesisBar::onOpenFolder);
        } else if (syncInfoMap.size() > 1) {
            foldersMenuItemWidget->setHasSubmenu(true);

            // Open folders submenu
            auto *submenu = new MenuWidget(MenuWidget::Submenu, menu);

            auto *openFolderActionGroup = new QActionGroup(this);
            openFolderActionGroup->setExclusive(true);

            for (auto const &[syncId, syncInfo]: syncInfoMap) {
                auto *openFolderAction = new QWidgetAction(this);
                openFolderAction->setProperty(MenuWidget::actionTypeProperty.c_str(), syncId);
                auto *openFolderMenuItemWidget = new MenuItemWidget(syncInfo.name());
                openFolderMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/folder.svg");
                openFolderAction->setDefaultWidget(openFolderMenuItemWidget);
                (void) connect(openFolderAction, &QWidgetAction::triggered, this, &SynthesisBar::onOpenFolder);
                openFolderActionGroup->addAction(openFolderAction);
            }

            submenu->addActions(openFolderActionGroup->actions());
            foldersMenuAction->setMenu(submenu);
        } else {
            // Sonar compliant
        }

        menu->addAction(foldersMenuAction);
    }

    // Open web version
    auto *driveOpenWebViewAction = new QWidgetAction(this);
    auto *driveOpenWebViewMenuItemWidget = new MenuItemWidget(tr("Open %1 web version").arg(APPLICATION_NAME));
    driveOpenWebViewMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/webview.svg");
    driveOpenWebViewAction->setDefaultWidget(driveOpenWebViewMenuItemWidget);
    driveOpenWebViewAction->setVisible(_gui->currentDriveDbId() != 0);
    (void) connect(driveOpenWebViewAction, &QWidgetAction::triggered, this, &SynthesisBar::onOpenWebview);
    menu->addAction(driveOpenWebViewAction);

    // Drive parameters
    if (!_gui->driveInfoMap().empty()) {
        auto *driveParametersAction = new QWidgetAction(this);
        auto *driveParametersMenuItemWidget = new MenuItemWidget(tr("Drive parameters"));
        driveParametersMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/drive.svg");
        driveParametersAction->setDefaultWidget(driveParametersMenuItemWidget);
        (void) connect(driveParametersAction, &QWidgetAction::triggered, this, &SynthesisBar::onOpenDriveParameters);
        menu->addAction(driveParametersAction);
    }

    // Disable Notifications
    auto *notificationsMenuAction = new QWidgetAction(this);
    const auto notificationAlreadyDisabledForPeriod =
            _notificationsDisabled != NotificationsDisabled::Never && _notificationsDisabled != NotificationsDisabled::Always;
    auto *notificationsMenuItemWidget =
            new MenuItemWidget(notificationAlreadyDisabledForPeriod
                                       ? tr("Notifications disabled until %1").arg(_notificationsDisabledUntilDateTime.toString())
                                       : tr("Disable Notifications"));
    notificationsMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/notification-off.svg");
    notificationsMenuItemWidget->setHasSubmenu(true);
    notificationsMenuAction->setDefaultWidget(notificationsMenuItemWidget);
    menu->addAction(notificationsMenuAction);

    // Disable Notifications submenu
    auto *submenu = new MenuWidget(MenuWidget::Submenu, menu);

    auto *notificationActionGroup = new QActionGroup(this);
    notificationActionGroup->setExclusive(true);

    const std::map<NotificationsDisabled, QString> &notificationMap =
            _notificationsDisabled == NotificationsDisabled::Never || _notificationsDisabled == NotificationsDisabled::Always
                    ? notificationsDisabledMap()
                    : notificationsDisabledForPeriodMap();

    for (auto const &[notifDisabled, str]: notificationMap) {
        auto *notificationAction = new QWidgetAction(this);
        notificationAction->setProperty(MenuWidget::actionTypeProperty.c_str(), toInt(notifDisabled));
        const QString text = QCoreApplication::translate("KDC::SynthesisPopover", str.toStdString().c_str());
        auto *notificationMenuItemWidget = new MenuItemWidget(text);
        notificationMenuItemWidget->setChecked(notifDisabled == _notificationsDisabled);
        notificationAction->setDefaultWidget(notificationMenuItemWidget);
        (void) connect(notificationAction, &QWidgetAction::triggered, this, &SynthesisBar::onNotificationActionTriggered);
        notificationActionGroup->addAction(notificationAction);
    }

    submenu->addActions(notificationActionGroup->actions());
    notificationsMenuAction->setMenu(submenu);

    // Application preferences
    auto *preferencesAction = new QWidgetAction(this);
    auto *preferencesMenuItemWidget = new MenuItemWidget(tr("Application preferences"));
    preferencesMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/parameters.svg");
    preferencesAction->setDefaultWidget(preferencesMenuItemWidget);
    (void) connect(preferencesAction, &QWidgetAction::triggered, this, &SynthesisBar::onOpenPreferences);
    menu->addAction(preferencesAction);

    // Help
    auto *helpAction = new QWidgetAction(this);
    auto *helpMenuItemWidget = new MenuItemWidget(tr("Need help"));
    helpMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/help.svg");
    helpAction->setDefaultWidget(helpMenuItemWidget);
    (void) connect(helpAction, &QWidgetAction::triggered, this, &SynthesisBar::onDisplayHelp);
    menu->addAction(helpAction);

    // Send feedbacks
    auto *feedbacksAction = new QWidgetAction(this);
    auto *feedbacksMenuItemWidget = new MenuItemWidget(tr("Send feedbacks"));
    feedbacksMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/messages-bubble-square-typing.svg");
    feedbacksAction->setDefaultWidget(feedbacksMenuItemWidget);
    (void) connect(feedbacksAction, &QWidgetAction::triggered, this, &SynthesisBar::onSendFeedback);
    menu->addAction(feedbacksAction);

    // Quit
    auto *exitAction = new QWidgetAction(this);
    auto *exitMenuItemWidget = new MenuItemWidget(tr("Quit kDrive"));
    exitMenuItemWidget->setLeftIcon(":/client/resources/icons/actions/error-sync.svg");
    exitAction->setDefaultWidget(exitMenuItemWidget);
    (void) connect(exitAction, &QWidgetAction::triggered, this, &SynthesisBar::onExit);
    menu->addAction(exitAction);

    if (_debugCrash) {
        // Emulate a crash
        auto *crashAction = new QWidgetAction(this);
        auto *crashMenuItemWidget = new MenuItemWidget("Emulate a crash");
        crashAction->setDefaultWidget(crashMenuItemWidget);
        (void) connect(crashAction, &QWidgetAction::triggered, this, &SynthesisBar::onCrash);
        menu->addAction(crashAction);

        // Emulate a server crash
        auto *crashServerAction = new QWidgetAction(this);
        auto *crashServerMenuItemWidget = new MenuItemWidget("Emulate a server crash");
        crashServerAction->setDefaultWidget(crashServerMenuItemWidget);
        (void) connect(crashServerAction, &QWidgetAction::triggered, this, &SynthesisBar::onCrashServer);
        menu->addAction(crashServerAction);

        // Emulate an ENFORCE crash
        auto *crashEnforceAction = new QWidgetAction(this);
        auto *crashEnforceMenuItemWidget = new MenuItemWidget("Emulate an ENFORCE crash");
        crashEnforceAction->setDefaultWidget(crashEnforceMenuItemWidget);
        (void) connect(crashEnforceAction, &QWidgetAction::triggered, this, &SynthesisBar::onCrashEnforce);
        menu->addAction(crashEnforceAction);

        // Emulate a qFatal crash
        auto *crashFatalAction = new QWidgetAction(this);
        auto *crashFatalMenuItemWidget = new MenuItemWidget("Emulate a qFatal crash");
        crashFatalAction->setDefaultWidget(crashFatalMenuItemWidget);
        (void) connect(crashFatalAction, &QWidgetAction::triggered, this, &SynthesisBar::onCrashFatal);
        menu->addAction(crashFatalAction);
    }

    menu->exec(QWidget::mapToGlobal(_menuButton->geometry().center()));
}

void SynthesisBar::onOpenFolder() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "openDriveFolder");
    const auto syncDbId = qvariant_cast<int>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    openUrl(syncDbId);
}

void SynthesisBar::onOpenWebview() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "openWebVersionButton");
    if (_gui->currentDriveDbId() != 0) {
        const auto driveInfoIt = _gui->driveInfoMap().find(_gui->currentDriveDbId());
        if (driveInfoIt == _gui->driveInfoMap().end()) {
            qCWarning(lcSynthesisBar()) << "Drive not found in drive map for driveDbId=" << _gui->currentDriveDbId();
            return;
        }

        QString driveLink;
        _gui->getWebviewDriveLink(_gui->currentDriveDbId(), driveLink);
        if (driveLink.isEmpty()) {
            qCWarning(lcSynthesisBar) << "Error in onOpenWebviewDrive " << _gui->currentDriveDbId();
            return;
        }

        if (!QDesktopServices::openUrl(driveLink)) {
            qCWarning(lcSynthesisBar) << "QDesktopServices::openUrl failed for " << driveLink;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to access web site %1.").arg(driveLink), QMessageBox::Ok,
                                    this);
            msgBox.exec();
        }
    }
}

void SynthesisBar::onOpenDriveParameters() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "driveParametersButton");
    emit showParametersDialog(_gui->currentDriveDbId());
}

void SynthesisBar::onNotificationActionTriggered() {
    const bool notificationAlreadyDisabledForPeriod =
            _notificationsDisabled != NotificationsDisabled::Never && _notificationsDisabled != NotificationsDisabled::Always;

    _notificationsDisabled = qvariant_cast<NotificationsDisabled>(sender()->property(MenuWidget::actionTypeProperty.c_str()));
    switch (_notificationsDisabled) {
        case NotificationsDisabled::Never:
            _notificationsDisabledUntilDateTime = QDateTime();
            break;
        case NotificationsDisabled::OneHour:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                          ? _notificationsDisabledUntilDateTime.addSecs(60 * 60)
                                                          : QDateTime::currentDateTime().addSecs(60 * 60);
            break;
        case NotificationsDisabled::UntilTomorrow:
            _notificationsDisabledUntilDateTime = QDateTime(QDateTime::currentDateTime().addDays(1).date(), QTime(8, 0));
            break;
        case NotificationsDisabled::ThreeDays:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                          ? _notificationsDisabledUntilDateTime.addDays(3)
                                                          : QDateTime::currentDateTime().addDays(3);
            break;
        case NotificationsDisabled::OneWeek:
            _notificationsDisabledUntilDateTime = notificationAlreadyDisabledForPeriod
                                                          ? _notificationsDisabledUntilDateTime.addDays(7)
                                                          : QDateTime::currentDateTime().addDays(7);
            break;
        case NotificationsDisabled::Always:
            _notificationsDisabledUntilDateTime = QDateTime();
            break;

        case NotificationsDisabled::EnumEnd: {
            assert(false && "Invalid enum value in switch statement.");
        }
    }
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "disableNotificationsFor",
                            static_cast<int>(_notificationsDisabled));
    emit disableNotifications(_notificationsDisabled, _notificationsDisabledUntilDateTime);
}

void SynthesisBar::onOpenPreferences() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "preferencesButton");
    emit showParametersDialog();
}

void SynthesisBar::onDisplayHelp() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "helpButton");
    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void SynthesisBar::onSendFeedback() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "sendFeedbackButton");
    const auto url = QUrl(Theme::instance()->feedbackUrl(ParametersCache::instance()->parametersInfo().language()));
    QDesktopServices::openUrl(url);
}

void SynthesisBar::onExit() {
    MatomoClient::sendEvent("synthesisKebab", MatomoEventAction::Click, "exitButton");
    hide();
    emit exit();
}

void SynthesisBar::onCrash() {
    emit crash();
}

void SynthesisBar::onCrashServer() {
    emit crashServer();
}

void SynthesisBar::onCrashEnforce() {
    emit crashEnforce();
}

void SynthesisBar::onCrashFatal() {
    emit crashFatal();
}

void SynthesisBar::retranslateUi() {
    _errorsButton->setToolTip(tr("Show errors and informations"));
    _infosButton->setToolTip(tr("Show informations"));
    _menuButton->setToolTip(tr("More actions"));
}

} // namespace KDC
