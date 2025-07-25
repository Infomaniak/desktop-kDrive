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

#include "adddrivelocalfolderwidget.h"
#include "custommessagebox.h"
#include "customtoolbutton.h"
#include "guiutility.h"
#include "config.h"
#include "enablestateholder.h"
#include "guirequests.h"
#include "clientgui.h"
#include "libcommongui/matomoclient.h"
#include "libcommon/utility/utility.h"

#include <QBoxLayout>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QLoggingCategory>
#include <QProgressBar>

namespace KDC {

static const int boxHSpacing = 10;
static const int logoBoxVMargin = 20;
static const int progressBarVMargin = 40;
static const int hLogoSpacing = 20;
static const int logoIconSize = 39;
static const QSize logoTextIconSize = QSize(60, 42);
static const int titleBoxVMargin = 30;
static const int selectionBoxHMargin = 15;
static const int selectionBoxVMargin = 20;
static const int selectionBoxSpacing = 10;
static const int selectionWidgetVMargin = 30;
static const int infoBoxSpacing = 10;
static const int infoWidgetVMargin = 25;
static const int progressBarMin = 0;
static const int progressBarMax = 5;

Q_LOGGING_CATEGORY(lcAddDriveLocalFolderWidget, "gui.adddrivelocalfolderwidget", QtInfoMsg)

AddDriveLocalFolderWidget::AddDriveLocalFolderWidget(std::shared_ptr<ClientGui> gui, QWidget *parent) :
    QWidget(parent),
    _gui(gui) {
    initUI();
    updateUI();
}

void AddDriveLocalFolderWidget::setDrive(const QString &driveName) {
    _titleLabel->setText(tr("Location of your %1 kDrive").arg(driveName));
}

void AddDriveLocalFolderWidget::setLocalFolderPath(const QString &path) {
    _localFolderPath = path;
    _defaultLocalFolderPath = path;
    updateUI();
}

void AddDriveLocalFolderWidget::setButtonIcon(const QColor &value) {
    if (_backButton) {
        _backButton->setIcon(KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/chevron-left.svg", value));
    }
}

void AddDriveLocalFolderWidget::initUI() {
    installEventFilter(this);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);

    // Logo
    QHBoxLayout *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(0, 0, 0, 0);
    mainLayout->addLayout(logoHBox);
    mainLayout->addSpacing(logoBoxVMargin);

    QLabel *logoIconLabel = new QLabel(this);
    logoIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-without-text.svg")
                                     .pixmap(QSize(logoIconSize, logoIconSize)));
    logoHBox->addWidget(logoIconLabel);
    logoHBox->addSpacing(hLogoSpacing);

    _logoTextIconLabel = new QLabel(this);
    logoHBox->addWidget(_logoTextIconLabel);
    logoHBox->addStretch();

    // Progress bar
    QProgressBar *progressBar = new QProgressBar(this);
    progressBar->setMinimum(progressBarMin);
    progressBar->setMaximum(progressBarMax);
    progressBar->setValue(3);
    progressBar->setFormat(QString());
    mainLayout->addWidget(progressBar);
    mainLayout->addSpacing(progressBarVMargin);

    // Title
    _titleLabel = new QLabel(this);
    _titleLabel->setObjectName("titleLabel");
    _titleLabel->setContentsMargins(0, 0, 0, 0);
    _titleLabel->setWordWrap(true);
    mainLayout->addWidget(_titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Folder selected widget
    QWidget *folderSelectedWidget = new QWidget(this);
    folderSelectedWidget->setObjectName("folderSelectionWidget");
    mainLayout->addWidget(folderSelectedWidget);

    QHBoxLayout *folderSelectedHBox = new QHBoxLayout();
    folderSelectedHBox->setContentsMargins(selectionBoxHMargin, selectionBoxVMargin, selectionBoxHMargin, selectionBoxVMargin);
    folderSelectedWidget->setLayout(folderSelectedHBox);

    QVBoxLayout *folderSelectedVBox = new QVBoxLayout();
    folderSelectedVBox->setContentsMargins(0, 0, 0, 0);
    folderSelectedHBox->addLayout(folderSelectedVBox);
    folderSelectedHBox->addStretch();

    QHBoxLayout *folderNameSelectedHBox = new QHBoxLayout();
    folderNameSelectedHBox->setContentsMargins(0, 0, 0, 0);
    folderNameSelectedHBox->setSpacing(selectionBoxSpacing);
    folderSelectedVBox->addLayout(folderNameSelectedHBox);

    _folderIconLabel = new QLabel(this);
    folderNameSelectedHBox->addWidget(_folderIconLabel);

    _folderNameLabel = new QLabel(this);
    _folderNameLabel->setObjectName("folderNameLabel");
    folderNameSelectedHBox->addWidget(_folderNameLabel);
    folderNameSelectedHBox->addStretch();

    _folderPathLabel = new QLabel(this);
    _folderPathLabel->setObjectName("folderpathlabel");
    _folderPathLabel->setContextMenuPolicy(Qt::PreventContextMenu);
    folderSelectedVBox->addWidget(_folderPathLabel);

    CustomToolButton *updateButton = new CustomToolButton(this);
    updateButton->setIconPath(":/client/resources/icons/actions/edit.svg");
    updateButton->setToolTip(tr("Edit folder"));
    folderSelectedHBox->addWidget(updateButton);
    mainLayout->addSpacing(selectionWidgetVMargin);

    // Info
    _infoWidget = new QWidget(this);
    _infoWidget->setVisible(false);
    mainLayout->addWidget(_infoWidget);

    QVBoxLayout *infoVBox = new QVBoxLayout();
    infoVBox->setContentsMargins(0, 0, 0, 0);
    _infoWidget->setLayout(infoVBox);

    QHBoxLayout *infoHBox = new QHBoxLayout();
    infoHBox->setContentsMargins(0, 0, 0, 0);
    infoHBox->setSpacing(infoBoxSpacing);
    infoVBox->addLayout(infoHBox);
    infoVBox->addSpacing(infoWidgetVMargin);

    _infoIconLabel = new QLabel(this);
    infoHBox->addWidget(_infoIconLabel);

    _infoLabel = new QLabel(this);
    _infoLabel->setObjectName("largeMediumTextLabel");
    _infoLabel->setWordWrap(true);
    infoHBox->addWidget(_infoLabel);
    infoHBox->setStretchFactor(_infoLabel, 1);

    // Warning
    _warningWidget = new QWidget(this);
    _warningWidget->setVisible(false);
    mainLayout->addWidget(_warningWidget);

    QVBoxLayout *warningVBox = new QVBoxLayout();
    warningVBox->setContentsMargins(0, 0, 0, 0);
    _warningWidget->setLayout(warningVBox);

    QHBoxLayout *warningHBox = new QHBoxLayout();
    warningHBox->setContentsMargins(0, 0, 0, 0);
    warningHBox->setSpacing(infoBoxSpacing);
    warningVBox->addLayout(warningHBox);
    warningVBox->addSpacing(infoWidgetVMargin);

    _warningIconLabel = new QLabel(this);
    warningHBox->addWidget(_warningIconLabel);

    _warningLabel = new QLabel(this);
    _warningLabel->setObjectName("largeMediumTextLabel");
    _warningLabel->setWordWrap(true);
    warningHBox->addWidget(_warningLabel);
    warningHBox->setStretchFactor(_warningLabel, 1);

    // Description
    QLabel *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName("largeNormalTextLabel");
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setText(
            tr("You will find all your files in this folder when the configuration is complete."
               " You can drop new files there to sync them to your kDrive."));
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addStretch();

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _backButton = new QPushButton(this);
    _backButton->setObjectName("nondefaultbutton");
    _backButton->setFlat(true);
    buttonsHBox->addWidget(_backButton);
    buttonsHBox->addStretch();

    _endButton = new QPushButton(this);
    _endButton->setObjectName("defaultbutton");
    _endButton->setFlat(true);

    bool skipExtSetup = true;
#ifdef Q_OS_MAC
    // Check LiteSync ext authorizations
    std::string liteSyncExtErrorDescr;
    bool liteSyncExtOk =
            CommonUtility::isLiteSyncExtEnabled() && CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
    if (!liteSyncExtErrorDescr.empty()) {
        qCWarning(lcAddDriveLocalFolderWidget)
                << "Error in CommonUtility::isLiteSyncExtFullDiskAccessAuthOk: " << liteSyncExtErrorDescr.c_str();
    }
    skipExtSetup = liteSyncExtOk;
#endif

    _endButton->setText(skipExtSetup ? tr("END") : tr("CONTINUE"));
    buttonsHBox->addWidget(_endButton);

    connect(updateButton, &CustomToolButton::clicked, this, &AddDriveLocalFolderWidget::onUpdateFolderButtonTriggered);
    connect(_folderPathLabel, &QLabel::linkActivated, this, &AddDriveLocalFolderWidget::onLinkActivated);
    connect(_warningLabel, &QLabel::linkActivated, this, &AddDriveLocalFolderWidget::onLinkActivated);
    connect(_backButton, &QPushButton::clicked, this, &AddDriveLocalFolderWidget::onBackButtonTriggered);
    connect(_endButton, &QPushButton::clicked, this, &AddDriveLocalFolderWidget::onContinueButtonTriggered);
}

void AddDriveLocalFolderWidget::updateUI() {
    if (const bool ok = !_localFolderPath.isEmpty(); !ok) return;

    const QDir dir(_localFolderPath);
    _folderNameLabel->setText(dir.dirName());
    _folderPathLabel->setText(QString(R"(<a style="%1" href="ref">%2</a>)").arg(CommonUtility::linkStyle, _localFolderPath));

    if (_localFolderPath != _defaultLocalFolderPath) {
        _infoLabel->setText(tr("The contents of the <b>%1</b> folder will be synchronized in your kDrive").arg(dir.dirName()));
        _infoWidget->setVisible(true);
    } else {
        _infoWidget->setVisible(false);
    }

    if (_liteSync) {
        VirtualFileMode virtualFileMode;
        const ExitCode exitCode = GuiRequests::bestAvailableVfsMode(virtualFileMode);
        if (exitCode != ExitCode::Ok) {
            qCWarning(lcAddDriveLocalFolderWidget) << "Error in Requests::bestAvailableVfsMode";
            return;
        }

        if (virtualFileMode == VirtualFileMode::Win || virtualFileMode == VirtualFileMode::Mac) {
            // Check file system
            _folderCompatibleWithLiteSync =
                    (virtualFileMode == VirtualFileMode::Win && CommonUtility::isNTFS(QStr2Path(_localFolderPath))) ||
                    (virtualFileMode == VirtualFileMode::Mac && CommonUtility::isAPFS(QStr2Path(_localFolderPath)));
            if (!_folderCompatibleWithLiteSync) {
                _warningLabel->setText(tr(R"(This folder is not compatible with Lite Sync.<br> 
Please select another folder. If you continue Lite Sync will be disabled.<br> 
<a style="%1" href="%2">Learn more</a>)")
                                               .arg(CommonUtility::linkStyle, KDC::GuiUtility::learnMoreLink));
                _warningWidget->setVisible(true);
            } else {
                _warningWidget->setVisible(false);
            }
        }
    }
}

void AddDriveLocalFolderWidget::selectFolder(const QString &startDirPath) {
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select folder"), startDirPath,
                                                        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dirPath.isEmpty()) {
        if (!GuiUtility::warnOnInvalidSyncFolder(dirPath, _gui->syncInfoMap(), this)) {
            return;
        }

        QDir dir(dirPath);
        _localFolderPath = dir.canonicalPath();
        updateUI();
    }
}

void AddDriveLocalFolderWidget::setFolderIcon() {
    if (_folderIconColor != QColor() && _folderIconSize != QSize()) {
        _folderIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/folder.svg", _folderIconColor)
                        .pixmap(_folderIconSize));
    }
}

void AddDriveLocalFolderWidget::setInfoIcon() {
    if (_infoIconColor != QColor() && _infoIconSize != QSize()) {
        _infoIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/information.svg", _infoIconColor)
                        .pixmap(_infoIconSize));
    }
}

void AddDriveLocalFolderWidget::setWarningIcon() {
    if (_warningIconColor != QColor() && _warningIconSize != QSize()) {
        _warningIconLabel->setPixmap(
                KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/warning.svg", _warningIconColor)
                        .pixmap(_warningIconSize));
    }
}

void AddDriveLocalFolderWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor)
                                          .pixmap(logoTextIconSize));
}

void AddDriveLocalFolderWidget::onDisplayMessage(const QString &text) {
    CustomMessageBox msgBox(QMessageBox::Warning, text, QMessageBox::Ok, this);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
}

void AddDriveLocalFolderWidget::onNeedToSave() {
    _needToSave = true;
}

void AddDriveLocalFolderWidget::onBackButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("addDriveLocalFolder", MatomoEventAction::Click, "backButton");

    emit terminated(false);
}

void AddDriveLocalFolderWidget::onContinueButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("addDriveLocalFolder", MatomoEventAction::Click, "continueButton");

    emit terminated();
}

void AddDriveLocalFolderWidget::onUpdateFolderButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("addDriveLocalFolder", MatomoEventAction::Click, "editButton");
    EnableStateHolder _(this);

    selectFolder(_localFolderPath);
}

void AddDriveLocalFolderWidget::onLinkActivated(const QString &link) {
    if (link == KDC::GuiUtility::learnMoreLink) {
        // Learn more: Folder not compatible with Lite Sync
        if (!QDesktopServices::openUrl(QUrl(LEARNMORE_LITESYNC_COMPATIBILITY_URL))) {
            qCWarning(lcAddDriveLocalFolderWidget) << "QDesktopServices::openUrl failed for " << link;
            CustomMessageBox msgBox(QMessageBox::Warning, tr("Unable to open link %1.").arg(link), QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}

bool AddDriveLocalFolderWidget::eventFilter(QObject *o, QEvent *e) {
    switch (e->type()) {
        case QEvent::EnabledChange: {
            auto w = static_cast<QWidget *>(o);
            showDisabledOverlay(!w->isEnabled());
            return true;
        }
        default:
            break;
    }

    return QWidget::eventFilter(o, e);
}

void AddDriveLocalFolderWidget::showDisabledOverlay(bool showOverlay) {
    if (showOverlay) {
        if (_disabledOverlay) delete _disabledOverlay;
        _disabledOverlay = new DisabledOverlay(this);
        _disabledOverlay->setGeometry(KDC::GuiUtility::getTopLevelWidget(this)->geometry());
        _disabledOverlay->show();
    } else {
        _disabledOverlay->hide();
    }
}

} // namespace KDC
