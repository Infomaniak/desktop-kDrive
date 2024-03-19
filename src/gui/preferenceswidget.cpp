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

#include "preferenceswidget.h"
#include "clientgui.h"
#include "parameterscache.h"
#include "preferencesblocwidget.h"
#include "customswitch.h"
#include "customcombobox.h"
#include "debuggingdialog.h"
#include "fileexclusiondialog.h"
#include "proxyserverdialog.h"
#include "bandwidthdialog.h"
#include "aboutdialog.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "common/utility.h"
#include "languagechangefilter.h"
#include "version.h"
#include "config.h"
#include "updater/updaterclient.h"
#include "litesyncdialog.h"
#include "enablestateholder.h"
#include "libcommongui/logger.h"
#include "guirequests.h"
#include "libcommon/theme/theme.h"
#include "libcommon/theme/theme.h"
#include "libcommon/utility/utility.h"
#include "resourcesmanagerdialog.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QGraphicsColorizeEffect>
#include <QIntValidator>
#include <QLabel>

namespace KDC {

static const int boxHMargin = 20;
static const int boxVMargin = 20;
static const int boxSpacing = 12;
static const int textHSpacing = 10;
static const int amountLineEditWidth = 85;

static const QString debuggingFolderLink = "debuggingFolderLink";
static const QString versionLink = "versionLink";
static const QString releaseNoteLink = "releaseNoteLink";

static const QString englishCode = "en";
static const QString frenchCode = "fr";
static const QString germanCode = "de";
static const QString spanishCode = "es";
static const QString italianCode = "it";

Q_LOGGING_CATEGORY(lcPreferencesWidget, "gui.preferenceswidget", QtInfoMsg)

PreferencesWidget::PreferencesWidget(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : ParametersWidget(parent),
      _gui(gui),
      _folderConfirmationAmountLineEdit(nullptr),
      _updateButton(nullptr),
      _languageSelectorComboBox(nullptr),
      _generalLabel(nullptr),
      _folderConfirmationLabel(nullptr),
      _folderConfirmationAmountLabel(nullptr),
      _darkThemeLabel(nullptr),
      _monochromeLabel(nullptr),
      _launchAtStartupLabel(nullptr),
      _moveToTrashLabel(nullptr),
      _languageSelectorLabel(nullptr),
      _shortcutsLabel(nullptr),
      _advancedLabel(nullptr),
      _debuggingLabel(nullptr),
      _debuggingFolderLabel(nullptr),
      _filesToExcludeLabel(nullptr),
      _proxyServerLabel(nullptr)
      //    , _resourcesLabel(nullptr)
      ,
      _liteSyncLabel(nullptr),
      _versionLabel(nullptr),
      _updateStatusLabel(nullptr),
      _showReleaseNoteLabel(nullptr),
      _versionNumberLabel(nullptr),
      _displayErrorsWidget(nullptr) {
    setContentsMargins(0, 0, 0, 0);

    /*
     *  vBox
     *      _displayErrorsWidget
     *      generalLabel
     *      generalBloc
     *          folderConfirmationBox
     *              folderConfirmation1HBox
     *                  folderConfirmationLabel
     *                  folderConfirmationSwitch
     *              folderConfirmation2HBox
     *                  _folderConfirmationAmountLineEdit
     *                  folderConfirmationAmountLabel
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
     *          bandwidthWidget
     *              bandwidthVBox
     *                  bandwidthLabel
     *      versionLabel
     *      versionBloc
     *          versionBox
     *              versionVBox
     *                  versionNumberLabel
     *                  copyrightLabel
     *              _updateButton
     */

    QVBoxLayout *vBox = new QVBoxLayout();
    vBox->setContentsMargins(boxHMargin, boxVMargin, boxHMargin, boxVMargin);
    vBox->setSpacing(boxSpacing);
    setLayout(vBox);

    //
    // Application errors
    //
    _displayErrorsWidget = new ActionWidget(":/client/resources/icons/actions/warning.svg", QString(), this);
    _displayErrorsWidget->setObjectName("displayErrorsWidget");
    vBox->addWidget(_displayErrorsWidget);
    _displayErrorsWidget->setVisible(false);

    //
    // General bloc
    //
    _generalLabel = new QLabel(this);
    _generalLabel->setObjectName("blocLabel");
    vBox->addWidget(_generalLabel);

    PreferencesBlocWidget *generalBloc = new PreferencesBlocWidget(this);
    vBox->addWidget(generalBloc);

    // Folder synchronization confirmation
    QBoxLayout *folderConfirmationBox = generalBloc->addLayout(QBoxLayout::Direction::TopToBottom);

    QHBoxLayout *folderConfirmation1HBox = new QHBoxLayout();
    folderConfirmation1HBox->setContentsMargins(0, 0, 0, 0);
    folderConfirmation1HBox->setSpacing(0);
    folderConfirmationBox->addLayout(folderConfirmation1HBox);

    _folderConfirmationLabel = new QLabel(this);
    _folderConfirmationLabel->setWordWrap(true);
    folderConfirmation1HBox->addWidget(_folderConfirmationLabel);
    folderConfirmation1HBox->setStretchFactor(_folderConfirmationLabel, 1);

    CustomSwitch *folderConfirmationSwitch = new CustomSwitch(this);
    folderConfirmationSwitch->setLayoutDirection(Qt::RightToLeft);
    folderConfirmationSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    folderConfirmationSwitch->setCheckState(
        ParametersCache::instance()->parametersInfo().useBigFolderSizeLimit() ? Qt::Checked : Qt::Unchecked);
    folderConfirmation1HBox->addWidget(folderConfirmationSwitch);

    QHBoxLayout *folderConfirmation2HBox = new QHBoxLayout();
    folderConfirmation2HBox->setContentsMargins(0, 0, 0, 0);
    folderConfirmation2HBox->setSpacing(textHSpacing);
    folderConfirmationBox->addLayout(folderConfirmation2HBox);

    _folderConfirmationAmountLineEdit = new QLineEdit(this);
    _folderConfirmationAmountLineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    _folderConfirmationAmountLineEdit->setEnabled(ParametersCache::instance()->parametersInfo().useBigFolderSizeLimit());
    _folderConfirmationAmountLineEdit->setText(
        QString::number(ParametersCache::instance()->parametersInfo().bigFolderSizeLimit()));
    _folderConfirmationAmountLineEdit->setValidator(new QIntValidator(0, 999999, this));
    _folderConfirmationAmountLineEdit->setMinimumWidth(amountLineEditWidth);
    _folderConfirmationAmountLineEdit->setMaximumWidth(amountLineEditWidth);
    folderConfirmation2HBox->addWidget(_folderConfirmationAmountLineEdit);

    _folderConfirmationAmountLabel = new QLabel(this);
    _folderConfirmationAmountLabel->setObjectName("folderConfirmationAmountLabel");
    folderConfirmation2HBox->addWidget(_folderConfirmationAmountLabel);
    folderConfirmation2HBox->addStretch();
    generalBloc->addSeparator();

    // Dark theme activation
    CustomSwitch *darkThemeSwitch = nullptr;
    if (!OldUtility::isMac()) {
        QBoxLayout *darkThemeBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

        _darkThemeLabel = new QLabel(this);
        darkThemeBox->addWidget(_darkThemeLabel);
        darkThemeBox->addStretch();

        darkThemeSwitch = new CustomSwitch(this);
        darkThemeSwitch->setLayoutDirection(Qt::RightToLeft);
        darkThemeSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
        darkThemeSwitch->setCheckState(ParametersCache::instance()->parametersInfo().darkTheme() ? Qt::Checked : Qt::Unchecked);
        darkThemeBox->addWidget(darkThemeSwitch);
        generalBloc->addSeparator();
    }

    // Monochrome icons activation
    QBoxLayout *monochromeIconsBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    _monochromeLabel = new QLabel(this);
    monochromeIconsBox->addWidget(_monochromeLabel);
    monochromeIconsBox->addStretch();

    CustomSwitch *monochromeSwitch = new CustomSwitch(this);
    monochromeSwitch->setLayoutDirection(Qt::RightToLeft);
    monochromeSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    monochromeSwitch->setCheckState(ParametersCache::instance()->parametersInfo().monoIcons() ? Qt::Checked : Qt::Unchecked);
    monochromeIconsBox->addWidget(monochromeSwitch);
    generalBloc->addSeparator();

    // Launch kDrive at startup
    QBoxLayout *launchAtStartupBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    _launchAtStartupLabel = new QLabel(this);
    launchAtStartupBox->addWidget(_launchAtStartupLabel);
    launchAtStartupBox->addStretch();

    CustomSwitch *launchAtStartupSwitch = new CustomSwitch(this);
    launchAtStartupSwitch->setLayoutDirection(Qt::RightToLeft);
    launchAtStartupSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    bool hasSystemLauchAtStartup = false;
    ExitCode exitCode = GuiRequests::hasSystemLaunchOnStartup(hasSystemLauchAtStartup);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::hasSystemLaunchOnStartup";
    }
    if (hasSystemLauchAtStartup) {
        // Cannot disable autostart because system-wide autostart is enabled
        launchAtStartupSwitch->setCheckState(Qt::Checked);
        launchAtStartupSwitch->setDisabled(true);
    } else {
        bool hasLaunchAtStartup = false;
        ExitCode exitCode = GuiRequests::hasLaunchOnStartup(hasLaunchAtStartup);
        if (exitCode != ExitCodeOk) {
            qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::hasLaunchOnStartup";
        }
        launchAtStartupSwitch->setCheckState(hasLaunchAtStartup ? Qt::Checked : Qt::Unchecked);
    }
    launchAtStartupBox->addWidget(launchAtStartupSwitch);
    generalBloc->addSeparator();

    // Move file to trash
    QBoxLayout *moveToTrashBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    _moveToTrashLabel = new QLabel(this);
    moveToTrashBox->addWidget(_moveToTrashLabel);
    moveToTrashBox->addStretch();

    CustomSwitch *moveToTrashSwitch = new CustomSwitch(this);
    moveToTrashSwitch->setLayoutDirection(Qt::RightToLeft);
    moveToTrashSwitch->setAttribute(Qt::WA_MacShowFocusRect, false);
    moveToTrashSwitch->setCheckState(ParametersCache::instance()->parametersInfo().moveToTrash() ? Qt::Checked : Qt::Unchecked);
    moveToTrashBox->addWidget(moveToTrashSwitch);
    generalBloc->addSeparator();

    // Languages
    QBoxLayout *languageSelectorBox = generalBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    _languageSelectorLabel = new QLabel(this);
    languageSelectorBox->addWidget(_languageSelectorLabel);
    languageSelectorBox->addStretch();

    _languageSelectorComboBox = new CustomComboBox(this);
    _languageSelectorComboBox->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    _languageSelectorComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _languageSelectorComboBox->setAttribute(Qt::WA_MacShowFocusRect, false);

    languageSelectorBox->addWidget(_languageSelectorComboBox);
    generalBloc->addSeparator();

#ifdef Q_OS_WIN
    // Drive shortcuts
    CustomSwitch *shortcutsSwitch = nullptr;
    if (OldUtility::isWindows()) {
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
    _advancedLabel = new QLabel(this);
    _advancedLabel->setObjectName("blocLabel");
    vBox->addWidget(_advancedLabel);

    PreferencesBlocWidget *advancedBloc = new PreferencesBlocWidget(this);
    vBox->addWidget(advancedBloc);

    // Debugging informations
    QVBoxLayout *debuggingVBox = nullptr;
    ClickableWidget *debuggingWidget = advancedBloc->addActionWidget(&debuggingVBox);

    _debuggingLabel = new QLabel(this);
    debuggingVBox->addWidget(_debuggingLabel);

    _debuggingFolderLabel = new QLabel(this);
    _debuggingFolderLabel->setVisible(ParametersCache::instance()->parametersInfo().useLog());
    _debuggingFolderLabel->setAttribute(Qt::WA_NoMousePropagation);
    _debuggingFolderLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    debuggingVBox->addWidget(_debuggingFolderLabel);
    advancedBloc->addSeparator();

    // Files to exclude
    QVBoxLayout *filesToExcludeVBox = nullptr;
    ClickableWidget *filesToExcludeWidget = advancedBloc->addActionWidget(&filesToExcludeVBox);

    _filesToExcludeLabel = new QLabel(this);
    filesToExcludeVBox->addWidget(_filesToExcludeLabel);
    advancedBloc->addSeparator();

    // Proxy server
    QVBoxLayout *proxyServerVBox = nullptr;
    ClickableWidget *proxyServerWidget = advancedBloc->addActionWidget(&proxyServerVBox);

    _proxyServerLabel = new QLabel(this);
    proxyServerVBox->addWidget(_proxyServerLabel);
    advancedBloc->addSeparator();

#ifdef Q_OS_MAC
    // Lite Sync
    QVBoxLayout *liteSyncVBox = nullptr;
    ClickableWidget *liteSyncWidget = advancedBloc->addActionWidget(&liteSyncVBox);

    _liteSyncLabel = new QLabel(this);
    liteSyncVBox->addWidget(_liteSyncLabel);
#endif

    //
    // Resources manager bloc
    //
    /*
        QVBoxLayout *resourcesManagerVBox = nullptr;
        ClickableWidget *resourcesManagerWidget = advancedBloc->addActionWidget(&resourcesManagerVBox);

        _resourcesLabel = new QLabel(this);
        _resourcesLabel->setObjectName("boldTextLabel");
        _resourcesLabel->setVisible(true);
        _resourcesLabel->setText(tr("Manage resources"));
        resourcesManagerVBox->addWidget(_resourcesLabel);
    */
    _versionLabel = new QLabel(this);
    _versionLabel->setObjectName("blocLabel");
    vBox->addWidget(_versionLabel);

    PreferencesBlocWidget *versionBloc = new PreferencesBlocWidget(this);
    vBox->addWidget(versionBloc);

    // Version
    QBoxLayout *versionBox = versionBloc->addLayout(QBoxLayout::Direction::LeftToRight);

    QVBoxLayout *versionVBox = new QVBoxLayout();
    versionVBox->setContentsMargins(0, 0, 0, 0);
    versionVBox->setSpacing(0);
    versionBox->addLayout(versionVBox);
    versionBox->setStretchFactor(versionVBox, 1);

    _updateStatusLabel = new QLabel(this);
    _updateStatusLabel->setObjectName("boldTextLabel");
    _updateStatusLabel->setWordWrap(true);
    _updateStatusLabel->setVisible(false);
    versionVBox->addWidget(_updateStatusLabel);

    _showReleaseNoteLabel = new QLabel(this);
    _showReleaseNoteLabel->setObjectName("boldTextLabel");
    _showReleaseNoteLabel->setWordWrap(true);
    _showReleaseNoteLabel->setVisible(false);
    versionVBox->addWidget(_showReleaseNoteLabel);

    _versionNumberLabel = new QLabel(
        tr("<a style=\"%1\" href=\"%2\">%3</a>").arg(CommonUtility::linkStyle, versionLink, KDRIVE_VERSION_STRING), this);
    _versionNumberLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    versionVBox->addWidget(_versionNumberLabel);

    QLabel *copyrightLabel = new QLabel(QString("Copyright %1").arg(APPLICATION_VENDOR), this);
    copyrightLabel->setObjectName("description");
    versionVBox->addWidget(copyrightLabel);

    _updateButton = new QPushButton(this);
    _updateButton->setObjectName("defaultbutton");
    _updateButton->setFlat(true);
    versionBox->addWidget(_updateButton);

    vBox->addStretch();

    // Init labels and setup connection for on the fly translation
    LanguageChangeFilter *languageFilter = new LanguageChangeFilter(this);
    installEventFilter(languageFilter);

    connect(folderConfirmationSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onFolderConfirmationSwitchClicked);
    connect(_folderConfirmationAmountLineEdit, &QLineEdit::textEdited, this,
            &PreferencesWidget::onFolderConfirmationAmountTextEdited);
    if (darkThemeSwitch) {
        connect(darkThemeSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onDarkThemeSwitchClicked);
    }
    connect(monochromeSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onMonochromeSwitchClicked);
    connect(launchAtStartupSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onLaunchAtStartupSwitchClicked);
    connect(_languageSelectorComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onLanguageChange()));
    connect(moveToTrashSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onMoveToTrashSwitchClicked);
#ifdef Q_OS_WIN
    connect(shortcutsSwitch, &CustomSwitch::clicked, this, &PreferencesWidget::onShortcutsSwitchClicked);
#endif
    connect(debuggingWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onDebuggingWidgetClicked);
    connect(_debuggingFolderLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
    connect(filesToExcludeWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onFilesToExcludeWidgetClicked);
    connect(proxyServerWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onProxyServerWidgetClicked);
//    connect(resourcesManagerWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onResourcesManagerWidgetClicked);
#ifdef Q_OS_MAC
    connect(liteSyncWidget, &ClickableWidget::clicked, this, &PreferencesWidget::onLiteSyncWidgetClicked);
#endif
    connect(_updateStatusLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
    connect(_showReleaseNoteLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
    connect(_versionNumberLabel, &QLabel::linkActivated, this, &PreferencesWidget::onLinkActivated);
    connect(_displayErrorsWidget, &ActionWidget::clicked, this, &PreferencesWidget::displayErrors);
}

void PreferencesWidget::showErrorBanner(bool unresolvedErrors) {
    _displayErrorsWidget->setVisible(unresolvedErrors);
}

void PreferencesWidget::showEvent(QShowEvent *event) {
    Q_UNUSED(event)

    retranslateUi();
    onUpdateInfo();
}

void PreferencesWidget::clearUndecidedLists() {
    for (const auto &syncInfoMapElt : _gui->syncInfoMap()) {
        // Clear the undecided list
        ExitCode exitCode = GuiRequests::setSyncIdSet(syncInfoMapElt.first, SyncNodeTypeUndecidedList, QSet<QString>());
        if (exitCode != ExitCodeOk) {
            qCWarning(lcPreferencesWidget()) << "Error in Requests::setSyncIdSet";
            return;
        }

        // Re-sync
        emit undecidedListsCleared();
        emit restartSync(syncInfoMapElt.first);
    }
}

void PreferencesWidget::updateStatus(QString status, bool updateAvailable) {
    if (status.isEmpty()) {
        _updateStatusLabel->setVisible(false);
        _showReleaseNoteLabel->setVisible(false);
    } else {
        _updateStatusLabel->setVisible(true);
        _updateStatusLabel->setText(status);

        if (updateAvailable) {
            _showReleaseNoteLabel->setVisible(true);
            QString releaseNoteLinkText =
                tr("<a style=\"%1\" href=\"%2\">Show release note</a>").arg(CommonUtility::linkStyle, releaseNoteLink);
            _showReleaseNoteLabel->setText(releaseNoteLinkText);
        }
    }
    _updateButton->setVisible(updateAvailable);
}

void PreferencesWidget::onFolderConfirmationSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setUseBigFolderSizeLimit(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    _folderConfirmationAmountLineEdit->setEnabled(checked);

    clearUndecidedLists();
}

void PreferencesWidget::onFolderConfirmationAmountTextEdited(const QString &text) {
    qint64 lValue = text.toLongLong();
    ParametersCache::instance()->parametersInfo().setBigFolderSizeLimit(lValue);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    clearUndecidedLists();
}

void PreferencesWidget::onDarkThemeSwitchClicked(bool checked) {
    emit setStyle(checked);
}

void PreferencesWidget::onMonochromeSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setMonoIcons(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    Theme::instance()->setSystrayUseMonoIcons(checked);
}

void PreferencesWidget::onLaunchAtStartupSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setAutoStart(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    ExitCode exitCode = GuiRequests::setLaunchOnStartup(checked);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcPreferencesWidget()) << "Error in GuiRequests::setLaunchOnStartup";
    }
}

void PreferencesWidget::onLanguageChange() {
    CustomComboBox *combo = qobject_cast<CustomComboBox *>(sender());
    if (!combo) return;

    Language language = static_cast<Language>(combo->currentData().toInt());

    ParametersCache::instance()->parametersInfo().setLanguage(language);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }

    CommonUtility::setupTranslations(QApplication::instance(), language);

    retranslateUi();
}

void PreferencesWidget::onMoveToTrashSwitchClicked(bool checked) {
    ParametersCache::instance()->parametersInfo().setMoveToTrash(checked);
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }
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
}
#endif

void PreferencesWidget::onDebuggingWidgetClicked() {
    EnableStateHolder _(this);

    DebuggingDialog dialog(_gui, this);
    dialog.exec();
    _debuggingFolderLabel->setVisible(ParametersCache::instance()->parametersInfo().useLog());
    repaint();
}

void PreferencesWidget::onFilesToExcludeWidgetClicked() {
    EnableStateHolder _(this);

    FileExclusionDialog dialog(this);
    dialog.exec();
}

void PreferencesWidget::onProxyServerWidgetClicked() {
    EnableStateHolder _(this);

    ProxyServerDialog dialog(this);
    dialog.exec();
}

void PreferencesWidget::onResourcesManagerWidgetClicked() {
    EnableStateHolder _(this);

    ResourcesManagerDialog dialog(this);
    dialog.exec();
}

void PreferencesWidget::onLiteSyncWidgetClicked() {
    EnableStateHolder _(this);

    LiteSyncDialog dialog(_gui, this);
    dialog.exec();
}

void PreferencesWidget::onLinkActivated(const QString &link) {
    if (link == debuggingFolderLink) {
        QString debuggingFolderPath = KDC::Logger::instance()->temporaryFolderLogDirPath();
        QUrl debuggingFolderUrl = KDC::GuiUtility::getUrlFromLocalPath(debuggingFolderPath);
        if (debuggingFolderUrl.isValid()) {
            if (!QDesktopServices::openUrl(debuggingFolderUrl)) {
                qCWarning(lcPreferencesWidget) << "QDesktopServices::openUrl failed for " << debuggingFolderUrl.toString();
                CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open folder %1.").arg(debuggingFolderUrl.toString()),
                                        QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
    } else if (link == versionLink) {
        EnableStateHolder _(this);

        AboutDialog dialog(this);
        dialog.execAndMoveToCenter(KDC::GuiUtility::getTopLevelWidget(this));
    } else if (link == releaseNoteLink) {
        QString version;
        try {
            version = UpdaterClient::instance()->version();
        } catch (std::exception const &) {
            return;
        }

        QString os;
#ifdef Q_OS_MAC
        os =
            "";  // In order to works with Sparkle, the URL must have the same name as the package. So do not add the os for macOS
#endif

#ifdef Q_OS_WIN
        os = "-win";
#endif

#ifdef Q_OS_LINUX
        os = "-linux";
#endif

        Language appLanguage = ParametersCache::instance()->parametersInfo().language();
        QString languageCode = KDC::CommonUtility::languageCode(appLanguage);

        if (KDC::CommonUtility::languageCodeIsEnglish(languageCode)) {
            QDesktopServices::openUrl(QUrl(QString("%1-%2%3.html").arg(APPLICATION_STORAGE_URL, version, os)));
        } else {
            QDesktopServices::openUrl(QUrl(QString("%1-%2%3-%4.html").arg(APPLICATION_STORAGE_URL, version, os, languageCode)));
        }
    } else {
        // URL link
        QUrl url = QUrl(link);
        if (url.isValid()) {
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

void PreferencesWidget::onUpdateInfo() {
    // Note: the sparkle-updater is not an KDCUpdater
    bool isKDCUpdater;
    try {
        isKDCUpdater = UpdaterClient::instance()->isKDCUpdater();
    } catch (std::exception const &) {
        return;
    }

#if defined(Q_OS_MAC)
    bool isSparkleUpdater;
    try {
        isSparkleUpdater = UpdaterClient::instance()->isSparkleUpdater();
    } catch (std::exception const &) {
        return;
    }
#endif

    QString statusString;
    try {
        statusString = UpdaterClient::instance()->statusString();
    } catch (std::exception const &) {
        return;
    }

    if (isKDCUpdater) {
        bool downloadCompleted;
        try {
            downloadCompleted = UpdaterClient::instance()->downloadCompleted();
        } catch (std::exception const &) {
            return;
        }

        connect(UpdaterClient::instance(), &UpdaterClient::downloadStateChanged, this, &PreferencesWidget::onUpdateInfo,
                Qt::UniqueConnection);
        connect(_updateButton, &QPushButton::clicked, this, &PreferencesWidget::onStartInstaller, Qt::UniqueConnection);
        connect(_updateButton, &QPushButton::clicked, qApp, &QApplication::quit, Qt::UniqueConnection);

        updateStatus(statusString, downloadCompleted);
    }
#if defined(Q_OS_MAC)
    else if (isSparkleUpdater) {
        bool updateFound;
        try {
            updateFound = UpdaterClient::instance()->updateFound();
        } catch (std::exception const &) {
            return;
        }

        connect(_updateButton, &QPushButton::clicked, this, &PreferencesWidget::onStartInstaller, Qt::UniqueConnection);

        updateStatus(statusString, updateFound);
    }
#endif
}

void PreferencesWidget::onStartInstaller() {
    try {
        UpdaterClient::instance()->startInstaller();
    } catch (std::exception const &) {
        // Do nothing
    }
}

void PreferencesWidget::retranslateUi() {
    _displayErrorsWidget->setText(tr("Some process failed to run."));
    _generalLabel->setText(tr("General"));
    _folderConfirmationLabel->setText(tr("Ask for confirmation before synchronizing folders greater than"));
    _folderConfirmationAmountLabel->setText(tr("MB"));
    if (_darkThemeLabel) {
        _darkThemeLabel->setText(tr("Activate dark theme"));
    }
    _monochromeLabel->setText(tr("Activate monochrome icons"));
    _launchAtStartupLabel->setText(tr("Launch kDrive at startup"));
    _languageSelectorLabel->setText(tr("Language"));
    _moveToTrashLabel->setText(tr("Move deleted files to computer's bin"));

    _languageSelectorComboBox->blockSignals(true);  // To avoid triggering more LanguageChange events
    _languageSelectorComboBox->clear();
    // QString systemLanguage = QLocale::system().uiLanguages().isEmpty() ? QString() : QLocale::system().uiLanguages().first();
    _languageSelectorComboBox->addItem(tr("Default"), LanguageDefault);
    _languageSelectorComboBox->addItem(tr("English"), LanguageEnglish);
    _languageSelectorComboBox->addItem(tr("French"), LanguageFrench);
    _languageSelectorComboBox->addItem(tr("German"), LanguageGerman);
    _languageSelectorComboBox->addItem(tr("Spanish"), LanguageSpanish);
    _languageSelectorComboBox->addItem(tr("Italian"), LanguageItalian);
    int languageIndex = _languageSelectorComboBox->findData(ParametersCache::instance()->parametersInfo().language());
    _languageSelectorComboBox->setCurrentIndex(languageIndex);
    _languageSelectorComboBox->blockSignals(false);

#ifdef Q_OS_WIN
    _shortcutsLabel->setText(tr("Show synchronized folders in File Explorer navigation pane"));
#endif
    _advancedLabel->setText(tr("Advanced"));
    _debuggingLabel->setText(tr("Debugging information"));
    _debuggingFolderLabel->setText(
        tr("<a style=\"%1\" href=\"%2\">Open debugging folder</a>").arg(CommonUtility::linkStyle, debuggingFolderLink));
    _filesToExcludeLabel->setText(tr("Files to exclude"));
    _proxyServerLabel->setText(tr("Proxy server"));
//    _bandwidthLabel->setText(tr("Bandwidth"));
#ifdef Q_OS_MAC
    _liteSyncLabel->setText(tr("Lite Sync"));
#endif
    _versionLabel->setText(tr("Version"));

    bool isKDCUpdater;
    try {
        isKDCUpdater = UpdaterClient::instance()->isKDCUpdater();
    } catch (std::exception const &) {
        return;
    }

#if defined(Q_OS_MAC)
    bool isSparkleUpdater;
    try {
        isSparkleUpdater = UpdaterClient::instance()->isSparkleUpdater();
    } catch (std::exception const &) {
        return;
    }
#endif

    QString statusString;
    try {
        statusString = UpdaterClient::instance()->statusString();
    } catch (std::exception const &) {
        return;
    }

    if (isKDCUpdater) {
        bool downloadCompleted;
        try {
            downloadCompleted = UpdaterClient::instance()->downloadCompleted();
        } catch (std::exception const &) {
            return;
        }

        updateStatus(statusString, downloadCompleted);
    }
#if defined(Q_OS_MAC)
    else if (isSparkleUpdater) {
        bool updateFound;
        try {
            updateFound = UpdaterClient::instance()->updateFound();
        } catch (std::exception const &) {
            return;
        }

        updateStatus(statusString, updateFound);
    }
#endif
    _updateButton->setText(tr("UPDATE"));
}

}  // namespace KDC
