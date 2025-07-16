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

#include "preferenceswidget.h"
#include "clientgui.h"
#include "parameterscache.h"
#include "preferencesblocwidget.h"
#include "customswitch.h"
#include "customcombobox.h"
#include "debuggingdialog.h"
#include "fileexclusiondialog.h"
#include "proxyserverdialog.h"
#include "aboutdialog.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "languagechangefilter.h"
#include "config.h"
#include "litesyncdialog.h"
#include "enablestateholder.h"
#include "libcommongui/logger.h"
#include "guirequests.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"

#ifdef Q_OS_WIN
#include "libcommon/info/parametersinfo.h"
#endif

#include <QBoxLayout>
#include <QDesktopServices>
#include <QGraphicsColorizeEffect>
#include <QIntValidator>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

namespace KDC {

static constexpr int boxHMargin = 20;
static constexpr int boxVMargin = 20;
static constexpr int boxSpacing = 12;
static constexpr int textHSpacing = 10;
static constexpr int amountLineEditWidth = 85;

static const QString debuggingFolderLink = "debuggingFolderLink";
static const QString moveToTrashFAQLink = "https://faq.infomaniak.com/2383#desktop";

static const QString englishCode = "en";
static const QString frenchCode = "fr";
static const QString germanCode = "de";
static const QString spanishCode = "es";
static const QString italianCode = "it";

Q_LOGGING_CATEGORY(lcPreferencesWidget, "gui.preferenceswidget", QtInfoMsg)

LargeFolderConfirmation::LargeFolderConfirmation(QBoxLayout *folderConfirmationBox) :
    _label{new QLabel()},
    _amountLabel{new QLabel()},
    _amountLineEdit{new QLineEdit()},
    _switch{new CustomSwitch()} {
    const auto folderConfirmation1HBox = new QHBoxLayout();
    folderConfirmation1HBox->setContentsMargins(0, 0, 0, 0);
    folderConfirmation1HBox->setSpacing(0);
    folderConfirmationBox->addLayout(folderConfirmation1HBox);

    _label->setWordWrap(true);
    folderConfirmation1HBox->addWidget(_label);
    folderConfirmation1HBox->setStretchFactor(_label, 1);

    _switch->setLayoutDirection(Qt::RightToLeft);
    _switch->setAttribute(Qt::WA_MacShowFocusRect, false);
    _switch->setCheckState(ParametersCache::instance()->parametersInfo().useBigFolderSizeLimit() ? Qt::Checked : Qt::Unchecked);
    folderConfirmation1HBox->addWidget(_switch);

    const auto folderConfirmation2HBox = new QHBoxLayout();
    folderConfirmation2HBox->setContentsMargins(0, 0, 0, 0);
    folderConfirmation2HBox->setSpacing(textHSpacing);
    folderConfirmationBox->addLayout(folderConfirmation2HBox);

    _amountLineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    _amountLineEdit->setEnabled(ParametersCache::instance()->parametersInfo().useBigFolderSizeLimit());
    _amountLineEdit->setText(QString::number(ParametersCache::instance()->parametersInfo().bigFolderSizeLimit()));
    _amountLineEdit->setValidator(new QIntValidator(0, 999999));
    _amountLineEdit->setMinimumWidth(amountLineEditWidth);
    _amountLineEdit->setMaximumWidth(amountLineEditWidth);
    folderConfirmation2HBox->addWidget(_amountLineEdit);

    _amountLabel->setObjectName("folderConfirmationAmountLabel");
    folderConfirmation2HBox->addWidget(_amountLabel);
    folderConfirmation2HBox->addStretch();
}

void LargeFolderConfirmation::retranslateUi() {
    _label->setText(tr("Ask for confirmation before synchronizing folders greater than"));
    _amountLabel->setText(tr("MB"));
}

void LargeFolderConfirmation::setAmountLineEditEnabled(bool enabled) {
    _amountLineEdit->setEnabled(enabled);
}

PreferencesWidget::PreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    LargeWidgetWithCustomToolTip(parent),
    _gui(gui),
    _languageSelectorComboBox{new CustomComboBox()},
    _generalLabel{new QLabel()},
    _monochromeLabel{new QLabel()},
    _launchAtStartupLabel{new QLabel()},
    _moveToTrashLabel{new QLabel()},
    _moveToTrashTipsLabel{new QLabel()},
    _moveToTrashDisclaimerLabel{new QLabel()},
    _moveToTrashKnowMoreLabel{new QLabel()},
    _languageSelectorLabel{new QLabel()},
    _advancedLabel{new QLabel()},
    _debuggingLabel{new QLabel()},
    _debuggingFolderLabel{new QLabel()},
    _filesToExcludeLabel{new QLabel()},
    _proxyServerLabel{new QLabel()},
    _displayErrorsWidget{new ActionWidget(":/client/resources/icons/actions/warning.svg", "")} {
    setContentsMargins(0, 0, 0, 0);

    /*
     *  vBox
     *      _displayErrorsWidget
     *      generalLabel
     *      generalBloc
     *          folderConfirmationBox
     *              folderConfirmation1HBox
     *                  _largeFolderConfirmation->_label
     *                  _largeFolderConfirmation->_switch
     *              folderConfirmation2HBox
     *                  _largeFolderConfirmation->_amountLineEdit
     *                  _largeFolderConfirmation->_amountLabel
     *          darkThemeBox
     *              darkThemeLabel
     *              darkThemeSwitch
     *          monochromeIconsBox
     *              monochromeLabel
     *              monochromeSwitch
     *          launchAtStartupBox
     *              launchAtStartupLabel
     *              launchAtStartupSwitch
     *          moveToTrashIconsBox
     *              moveToTrashLabel
     *              moveToTrashSwitch
     *          languageSelectorBox
     *              languageSelectorLabel
     *              languageSelectorComboBox
     *      advancedLabel
     *      advancedBloc
     *          debuggingWidget
     *              debuggingVBox
     *                  debuggingLabel
     *                  _debuggingFolderLabel
     *          filesToExcludeWidget
     *              filesToExcludeVBox
     *                  filesToExcludeLabel
     *          proxyServerWidget
     *              proxyServerVBox
     *                  proxyServerLabel
     *      _versionWidget->versionLabel
     *      versionBloc
     *          versionBox
     *              versionVBox
     *                  _versionWidget->_versionNumberLabel
     *                  _versionWidget->_showReleaseNoteLabel
     *                  copyrightLabel
     *                  _versionWidget->_updateStatusLabel
     *              _versionWidget->_updateButton
     */

    const auto vBox = new QVBoxLayout();
    vBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    vBox->setSpacing(boxSpacing);
    setLayout(vBox);

    //
    // Application errors
    //
    _displayErrorsWidget->setObjectName("displayErrorsWidget");
    vBox->addWidget(_displayErrorsWidget);
    _displayErrorsWidget->setVisible(false);

    //
    // General bloc
    //
    _generalLabel->setObjectName("blocLabel");
    vBox->addWidget(_generalLabel);

    const auto generalBloc = new PreferencesBlocWidget();
    vBox->addWidget(generalBloc);

    // Synchronization confirmation for large folders
    QBoxLayout *folderConfirmationBox = generalBloc->addLayout(QBoxLayout::Direction::TopToBottom);
    _largeFolderConfirmation = std::unique_ptr<LargeFolderConfirmation>(new LargeFolderConfirmation(folderConfirmationBox));

    generalBloc->addSeparator();

    // Dark theme activation
    CustomSwitch *darkThemeSwitch = nullptr;
    if (!CommonUtility::isMac()) {
        QBoxLayout *darkThemeBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

        _darkThemeLabel = new QLabel();
        darkThemeBox->addWidget(_darkThemeLabel);
        darkThemeBox->addStretch();

        darkThemeSwitch = new CustomSwitch();
        darkThemeSwitch->setLayoutDirection(Qt::RightToLeft);
        darkThemeSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
        darkThemeSwitch->setCheckState(ParametersCache::instance()->parametersInfo().darkTheme() ? Qt::Checked : Qt::Unchecked);
        darkThemeBox->addWidget(darkThemeSwitch);
        generalBloc->addSeparator();
    }

    // Monochrome icons activation
    QBoxLayout *monochromeIconsBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    monochromeIconsBox->addWidget(_monochromeLabel);
    monochromeIconsBox->addStretch();

    const auto monochromeSwitch = new CustomSwitch();
    monochromeSwitch->setLayoutDirection(Qt::RightToLeft);
    monochromeSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    monochromeSwitch->setCheckState(ParametersCache::instance()->parametersInfo().monoIcons() ? Qt::Checked : Qt::Unchecked);
    monochromeIconsBox->addWidget(monochromeSwitch);
    generalBloc->addSeparator();

    // Launch kDrive at startup
    QBoxLayout *launchAtStartupBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    launchAtStartupBox->addWidget(_launchAtStartupLabel);
    launchAtStartupBox->addStretch();

    const auto launchAtStartupSwitch = new CustomSwitch();
    launchAtStartupSwitch->setLayoutDirection(Qt::RightToLeft);
    launchAtStartupSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    bool hasSystemLauchAtStartup = false;
    ExitCode exitCode = GuiRequests::hasSystemLaunchOnStartup(hasSystemLauchAtStartup);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::hasSystemLaunchOnStartup";
    }
    if (hasSystemLauchAtStartup) {
        // Cannot disable autostart because system-wide autostart is enabled
        launchAtStartupSwitch->setCheckState(Qt::Checked);
        launchAtStartupSwitch->setDisabled(true);
    } else {
        bool hasLaunchAtStartup = false;
        ExitCode exitCode = GuiRequests::hasLaunchOnStartup(hasLaunchAtStartup);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::hasLaunchOnStartup";
        }
        launchAtStartupSwitch->setCheckState(hasLaunchAtStartup ? Qt::Checked : Qt::Unchecked);
    }
    launchAtStartupBox->addWidget(launchAtStartupSwitch);
    generalBloc->addSeparator();

    // Move file to trash
    const auto moveToTrashBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight, true);
    moveToTrashBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin / 2);
    const auto moveToTrashTitleBox = new QVBoxLayout();
    moveToTrashTitleBox->setContentsMargins(0, 0, 0, 0);
    _moveToTrashTipsLabel->setWordWrap(true);
    _moveToTrashTipsLabel->setObjectName("description");
    moveToTrashTitleBox->addWidget(_moveToTrashLabel);
    moveToTrashTitleBox->addWidget(_moveToTrashTipsLabel);
    moveToTrashBox->addLayout(moveToTrashTitleBox);

    const auto moveToTrashSwitch = new CustomSwitch();
    moveToTrashSwitch->setLayoutDirection(Qt::RightToLeft);
    moveToTrashSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    moveToTrashSwitch->setCheckState(ParametersCache::instance()->parametersInfo().moveToTrash() ? Qt::Checked : Qt::Unchecked);
    moveToTrashSwitch->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    moveToTrashBox->addWidget(moveToTrashSwitch);

    // Move file to trash disclaimer
    _moveTotrashDisclaimerWidget = new QWidget();
    _moveTotrashDisclaimerWidget->setObjectName("disclaimerWidget");
    _moveTotrashDisclaimerWidget->setVisible(moveToTrashSwitch->isChecked());
    const auto moveToTrashDisclaimerHBox = new QHBoxLayout(_moveTotrashDisclaimerWidget);
    moveToTrashDisclaimerHBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);

    const auto warningIconLabel = new QLabel();
    warningIconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", QColor(255, 140, 0))
                    .pixmap(20, 20));
    warningIconLabel->setContentsMargins(0, 0, textHSpacing, 0);
    moveToTrashDisclaimerHBox->addWidget(warningIconLabel);
    _moveToTrashDisclaimerLabel->setWordWrap(true);
    moveToTrashDisclaimerHBox->addWidget(_moveToTrashDisclaimerLabel);
    moveToTrashDisclaimerHBox->addStretch();
    moveToTrashDisclaimerHBox->addWidget(_moveToTrashKnowMoreLabel);
    moveToTrashDisclaimerHBox->setStretchFactor(_moveToTrashDisclaimerLabel, 1);
    const auto moveToTrashDisclaimerBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight, true);
    moveToTrashDisclaimerBox->setContentsMargins(boxHMargin, 0, boxHMargin, boxVMargin / 2);
    moveToTrashDisclaimerBox->addWidget(_moveTotrashDisclaimerWidget);
    generalBloc->addSeparator();

    // Languages
    const auto languageSelectorBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    languageSelectorBox->addWidget(_languageSelectorLabel);
    languageSelectorBox->addStretch();

    _languageSelectorComboBox->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    _languageSelectorComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _languageSelectorComboBox->setAttribute(Qt::WA_MacShowFocusRect, false);

    languageSelectorBox->addWidget(_languageSelectorComboBox);
    generalBloc->addSeparator();

#ifdef Q_OS_WIN
    // Drive shortcuts
    CustomSwitch *shortcutsSwitch = nullptr;
    if (CommonUtility::isWindows()) {
        QBoxLayout *shortcutsBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

        _shortcutsLabel = new QLabel(this);
        _shortcutsLabel->setWordWrap(true);
        shortcutsBox->addWidget(_shortcutsLabel);

        shortcutsSwitch = new CustomSwitch(this);
        shortcutsSwitch->setLayoutDirection(Qt::RightToLeft);
        shortcutsSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
        shortcutsSwitch->setCheckState(ParametersCache::instance()->parametersInfo().showShortcuts() ? Qt::Checked
                                                                                                     : Qt::Unchecked);
        shortcutsBox->addWidget(shortcutsSwitch);
        shortcutsBox->setStretchFactor(_shortcutsLabel, 1);
    }
#endif

    //
    // Advanced bloc
    //
    _advancedLabel->setObjectName("blocLabel");
    vBox->addWidget(_advancedLabel);

    const auto advancedBloc = new PreferencesBlocWidget();
    vBox->addWidget(advancedBloc);

    // Debugging information
    QVBoxLayout *debuggingVBox = nullptr;
    const auto debuggingWidget = advancedBloc->addActionWidget(&debuggingVBox);

    debuggingVBox->addWidget(_debuggingLabel);

    _debuggingFolderLabel->setVisible(ParametersCache::instance()->parametersInfo().useLog());
    _debuggingFolderLabel->setAttribute(Qt::WA_NoMousePropagation);
    _debuggingFolderLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    debuggingVBox->addWidget(_debuggingFolderLabel);
    advancedBloc->addSeparator();

    // Files to exclude
    QVBoxLayout *filesToExcludeVBox = nullptr;
    const auto filesToExcludeWidget = advancedBloc->addActionWidget(&filesToExcludeVBox);

    filesToExcludeVBox->addWidget(_filesToExcludeLabel);
    advancedBloc->addSeparator();

    // Proxy server
    QVBoxLayout *proxyServerVBox = nullptr;
    const auto proxyServerWidget = advancedBloc->addActionWidget(&proxyServerVBox);

    proxyServerVBox->addWidget(_proxyServerLabel);
    advancedBloc->addSeparator();

#ifdef Q_OS_MAC
    // Lite Sync
    QVBoxLayout *liteSyncVBox = nullptr;
    const auto liteSyncWidget = advancedBloc->addActionWidget(&liteSyncVBox);

    _liteSyncLabel = new QLabel();
    liteSyncVBox->addWidget(_liteSyncLabel);
#endif

    // Version
    _versionWidget = new VersionWidget(this);
    vBox->addWidget(_versionWidget);

    vBox->addStretch();

    // Init labels and setup connection for on the fly translation
    const auto languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);

    connect(_largeFolderConfirmation->customSwitch(), &CustomSwitch::clicked, this,
            &PreferencesWidget::onFolderConfirmationSwitchClicked);
    connect(_largeFolderConfirmation->amountLineEdit(), &QLineEdit::textEdited, this,
            &PreferencesWidget::onFolderConfirmationAmountTextEdited);
    if (darkThemeSwitch) {
        connect(darkThemeSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onDarkThemeSwitchClicked);
    }
    connect(monochromeSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onMonochromeSwitchClicked);
    connect(launchAtStartupSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onLaunchAtStartupSwitchClicked);
    connect(_languageSelectorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLanguageChange()));
    connect(moveToTrashSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onMoveToTrashSwitchClicked);
    connect(_moveToTrashKnowMoreLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
#ifdef Q_OS_WIN
    connect(shortcutsSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onShortcutsSwitchClicked);
#endif
    connect(debuggingWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onDebuggingWidgetClicked);
    connect(_debuggingFolderLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
    connect(filesToExcludeWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onFilesToExcludeWidgetClicked);
    connect(proxyServerWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onProxyServerWidgetClicked);
#ifdef Q_OS_MAC
    connect(liteSyncWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onLiteSyncWidgetClicked);
#endif

    connect(_gui.get(), &ClientGui::updateStateChanged, _versionWidget, &VersionWidget::onUpdateStateChanged);
    connect(_versionWidget, &VersionWidget::showUpdateDialog, _gui.get(), &ClientGui::onShowWindowsUpdateDialog,
            Qt::QueuedConnection);

    connect(_displayErrorsWidget, &ActionWidget::clicked, this, &PreferencesWidget::displayErrors);
}

void PreferencesWidget::showErrorBanner(const bool unresolvedErrors) const {
    _displayErrorsWidget->setVisible(unresolvedErrors);
}

void PreferencesWidget::showEvent(QShowEvent *event) {
    Q_UNUSED(event)
    retranslateUi();
    _versionWidget->refresh(isStaff());
}

void PreferencesWidget::clearUndecidedLists() {
    for (const auto &syncInfoMapElt: _gui->syncInfoMap()) {
        // Clear the undecided list
        ExitCode exitCode = GuiRequests::setSyncIdSet(syncInfoMapElt.first, SyncNodeType::UndecidedList, QSet<QString>());
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcPreferencesWidget()) << "Error in Requests::setSyncIdSet";
            return;
        }

        // Re-sync
        emit undecidedListsCleared();
        emit restartSync(syncInfoMapElt.first);
    }
}

bool PreferencesWidget::isStaff() const {
    constexpr auto isStaffCallback = [](std::pair<int, UserInfoClient> const &item) { return item.second.isStaff(); };
    // To be used with an later gcc version
    // return std::ranges::any_of(_gui->userInfoMap(), isStaffCallback);
    return std::any_of(_gui->userInfoMap().cbegin(), _gui->userInfoMap().cend(), isStaffCallback);
}

void PreferencesWidget::onFolderConfirmationSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setUseBigFolderSizeLimit(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    _largeFolderConfirmation->setAmountLineEditEnabled(checked);

    clearUndecidedLists();
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "largeFolderConfirmationSwitch", checked ? 1 : 0);
}

void PreferencesWidget::onFolderConfirmationAmountTextEdited(const QString &text) {
    const qint64 lValue = text.toLongLong();
    ParametersCache::instance()->parametersInfo().setBigFolderSizeLimit(lValue);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }
    // we can cast lvalue to an int because the max value is 999999
    MatomoClient::sendEvent("preferences", MatomoEventAction::Input, "largeFolderConfirmationValue", static_cast<int>(lValue));

    clearUndecidedLists();
}

void PreferencesWidget::onDarkThemeSwitchClicked(bool checked) {
    emit setStyle(checked);
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "darkThemeSwitch", checked ? 1 : 0);
}

void PreferencesWidget::onMonochromeSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setMonoIcons(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    Theme::instance()->setSystrayUseMonoIcons(checked);
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "monochromeIconsSwitch", checked ? 1 : 0);
}

void PreferencesWidget::onLaunchAtStartupSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setAutoStart(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    const ExitCode exitCode = GuiRequests::setLaunchOnStartup(checked);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::setLaunchOnStartup";
    }
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "launchAtStartupSwitch", checked ? 1 : 0);
}

void PreferencesWidget::onLanguageChange() {
    CustomComboBox *combo = qobject_cast<CustomComboBox *>(sender());
    if (!combo) return;

    const auto language = static_cast<Language>(combo->currentData().toInt());

    ParametersCache::instance()->parametersInfo().setLanguage(language);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "languagesChange", combo->currentData().toInt());

    CommonUtility::setupTranslations(QApplication::instance(), language);

    retranslateUi();
    _versionWidget->refresh(isStaff());
}

void PreferencesWidget::onMoveToTrashSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setMoveToTrash(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }
    _moveTotrashDisclaimerWidget->setVisible(checked);
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "moveToTrashSwitch", checked ? 1 : 0);
}

#ifdef Q_OS_WIN
void PreferencesWidget::onShortcutsSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setShowShortcuts(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    GuiRequests::setShowInExplorerNavigationPane(checked);
    CustomMessageBox msgBox(QMessageBox::Information,
                            tr("You must restart your opened File Explorers for this change to take effect."), QMessageBox::Ok,
                            this);
    msgBox.exec();
    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "windowsShortcutsSwitch", checked ? 1 : 0);
}
#endif

void PreferencesWidget::onDebuggingWidgetClicked() {
    EnableStateHolder _(this);

    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "debuggingPopup");
    DebuggingDialog dialog(_gui, this);
    dialog.exec();
    _debuggingFolderLabel->setVisible(ParametersCache::instance()->parametersInfo().useLog());
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_Debugging);
    repaint();
}

void PreferencesWidget::onFilesToExcludeWidgetClicked() {
    EnableStateHolder _(this);

    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "filesToExcludePopup");
    FileExclusionDialog dialog(this);
    dialog.exec();
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_FilesToExclude);
}

void PreferencesWidget::onProxyServerWidgetClicked() {
    EnableStateHolder _(this);

    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "proxyServerPopup");
    ProxyServerDialog dialog(this);
    dialog.exec();
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_Proxy);
}

void PreferencesWidget::onLiteSyncWidgetClicked() {
    EnableStateHolder _(this);

    MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "liteSyncPopup");
    LiteSyncDialog dialog(_gui, this);
    dialog.exec();
#ifdef Q_OS_MAC
    MatomoClient::sendVisit(MatomoNameField::PG_Preferences_LiteSync);
#endif
}

void PreferencesWidget::onLinkActivated(const QString &link) {
    if (link == debuggingFolderLink) {
        const QString debuggingFolderPath = KDC::Logger::instance()->temporaryFolderLogDirPath();
        const QUrl debuggingFolderUrl = KDC::GuiUtility::getUrlFromLocalPath(debuggingFolderPath);
        if (debuggingFolderUrl.isValid()) {
            MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "debuggingFolderLink");
            if (!QDesktopServices::openUrl(debuggingFolderUrl)) {
                qCWarning(lcPreferencesWidget) << "QDesktopServices::openUrl failed for " << debuggingFolderUrl.toString();
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(debuggingFolderUrl.toString()),
                                        QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
    } else {
        // URL link
        const auto url = QUrl(link);
        if (url.isValid()) {
            MatomoClient::sendEvent("preferences", MatomoEventAction::Click, "moveToTrashLearnMoreLink");
            if (!QDesktopServices::openUrl(url)) {
                qCWarning(lcPreferencesWidget) << "QDesktopServices::openUrl failed for " << link;
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(link), QMessageBox::Ok, this);
                msgBox.exec();
            }
        } else {
            qCWarning(lcPreferencesWidget) << "Invalid link " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Invalid link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

void PreferencesWidget::retranslateUi() const {
    _displayErrorsWidget->setText(tr("Some process failed to run."));
    _generalLabel->setText(tr("General"));
    _largeFolderConfirmation->retranslateUi();
    if (_darkThemeLabel) {
        _darkThemeLabel->setText(tr("Activate dark theme"));
    }
    _monochromeLabel->setText(tr("Activate monochrome icons"));
    _launchAtStartupLabel->setText(tr("Launch kDrive at startup"));
    _languageSelectorLabel->setText(tr("Language"));
    _moveToTrashLabel->setText(tr("Move deleted files to my computer's trash"));
    _moveToTrashDisclaimerLabel->setText(tr("Some files or folders may not be moved to the computer's trash."));
    _moveToTrashTipsLabel->setText(tr("You can always retrieve already synced files from the kDrive web application trash."));
    _moveToTrashKnowMoreLabel->setText(
            tr(R"(<a style="%1" href="%2">Learn more</a>)").arg(CommonUtility::linkStyle, LEARNMORE_MOVE_TO_TRASH_URL));
    _languageSelectorComboBox->blockSignals(true); // To avoid triggering more LanguageChange events
    _languageSelectorComboBox->clear();
    _languageSelectorComboBox->addItem(tr("Default"), toInt(Language::Default));
    _languageSelectorComboBox->addItem(tr("English"), toInt(Language::English));
    _languageSelectorComboBox->addItem(tr("French"), toInt(Language::French));
    _languageSelectorComboBox->addItem(tr("German"), toInt(Language::German));
    _languageSelectorComboBox->addItem(tr("Spanish"), toInt(Language::Spanish));
    _languageSelectorComboBox->addItem(tr("Italian"), toInt(Language::Italian));
    const int languageIndex =
            _languageSelectorComboBox->findData(toInt(ParametersCache::instance()->parametersInfo().language()));
    _languageSelectorComboBox->setCurrentIndex(languageIndex);
    _languageSelectorComboBox->blockSignals(false);

#ifdef Q_OS_WIN
    _shortcutsLabel->setText(tr("Show synchronized folders in File Explorer navigation pane"));
#endif
    _advancedLabel->setText(tr("Advanced"));
    _debuggingLabel->setText(tr("Debugging information"));
    _debuggingFolderLabel->setText(
            tr(R"(<a style="%1" href="%2">Open debugging folder</a>)").arg(CommonUtility::linkStyle, debuggingFolderLink));
    _filesToExcludeLabel->setText(tr("Files to exclude"));
    _proxyServerLabel->setText(tr("Proxy server"));
#ifdef Q_OS_MAC
    _liteSyncLabel->setText(tr("Lite Sync"));
#endif
}

} // namespace KDC
