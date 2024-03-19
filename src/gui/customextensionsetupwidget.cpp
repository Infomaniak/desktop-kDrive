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

#include "customextensionsetupwidget.h"
#include "guiutility.h"
#include "parameterscache.h"
#include "libcommon/utility/utility.h"

#include <QBoxLayout>
#include <QFileDialog>
#include <QLoggingCategory>

namespace KDC {

static const int logoBoxVMargin = 20;
static const int logoIconSize = 39;
static const int hLogoSpacing = 20;
static const int progressBarVMargin = 30;
static const int titleBoxVMargin = 20;
static const int descriptionVMargin = 20;
static const int boxHSpacing = 10;
static const QSize stepPictureSize = QSize(186, 117);
static const QSize stepIconSize = QSize(28, 28);
static const int stepIconHMargin = 5;
static const int stepVMargin = 15;
static const QSize logoTextIconSize = QSize(60, 42);
static const int progressBarMin = 0;
static const int progressBarMax = 5;


Q_LOGGING_CATEGORY(lcCustomExtensionSetupWidget, "gui.customextensionsetupwidget", QtInfoMsg)

CustomExtensionSetupWidget::CustomExtensionSetupWidget(QWidget *parent, bool addDriveSetup)
    : QWidget(parent),
      _finishedButton(nullptr),
      _logoTextIconLabel(nullptr),
      _descriptionLabel(nullptr),
      _logoColor(QColor()),
      _step1Label(nullptr),
      _step2Label(nullptr),
      _mainLayout(nullptr),
      _isAddDriveSetup(addDriveSetup) {
    _mainLayout = new QVBoxLayout();
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    _mainLayout->setSpacing(0);
    setLayout(_mainLayout);

    initUI();

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &CustomExtensionSetupWidget::onUpdateProgress);
    _timer->start(1000);
}

void CustomExtensionSetupWidget::addDriveMainLayoutInit() {
    // Logo
    QHBoxLayout *logoHBox = new QHBoxLayout();
    logoHBox->setContentsMargins(0, 0, 0, 0);
    _mainLayout->addLayout(logoHBox);
    _mainLayout->addSpacing(logoBoxVMargin);

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
    progressBar->setValue(4);
    progressBar->setFormat(QString());
    _mainLayout->addWidget(progressBar);
    _mainLayout->addSpacing(progressBarVMargin);

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setText(tr("Before finishing"));
    titleLabel->setContentsMargins(0, 0, 0, 0);
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignLeft);
    _mainLayout->addWidget(titleLabel);
    _mainLayout->addSpacing(titleBoxVMargin);

    // Description
    _descriptionLabel = new QLabel(this);
    _descriptionLabel->setObjectName("largeNormalTextLabel");
    _descriptionLabel->setText(
        tr("Perform the following steps to ensure that Lite Sync works correctly on your computer and to complete the "
           "configuration of the kDrive."));
    _descriptionLabel->setWordWrap(true);
    _descriptionLabel->setAlignment(Qt::AlignLeft);
    _mainLayout->addWidget(_descriptionLabel);
    _mainLayout->addSpacing(descriptionVMargin);
}

void CustomExtensionSetupWidget::dialogMainLayoutInit() {
    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(0, 0, 0, 0);
    titleLabel->setText(tr("Before finishing"));
    titleLabel->setWordWrap(true);
    titleLabel->setAlignment(Qt::AlignLeft);
    _mainLayout->addWidget(titleLabel);
    _mainLayout->addSpacing(titleBoxVMargin);

    // Description
    _descriptionLabel = new QLabel(this);
    _descriptionLabel->setObjectName("largeNormalTextLabel");
    _descriptionLabel->setContentsMargins(0, 0, 0, 0);
    _descriptionLabel->setText(
        tr("Perform the following steps to ensure that Lite Sync works correctly on your computer and to complete the "
           "configuration of the kDrive."));
    _descriptionLabel->setWordWrap(true);
    _descriptionLabel->setAlignment(Qt::AlignLeft);
    _mainLayout->addWidget(_descriptionLabel);
    _mainLayout->addSpacing(descriptionVMargin);
}

void CustomExtensionSetupWidget::initUI() {
    bool macOs13orLater = QSysInfo::productVersion().toDouble() >= 13.0;

    if (_isAddDriveSetup) {
        addDriveMainLayoutInit();
    } else {
        dialogMainLayoutInit();
    }

    // Step 1
    QVBoxLayout *step1VBox = new QVBoxLayout();
    step1VBox->setContentsMargins(0, 0, 0, 0);
    _mainLayout->addLayout(step1VBox);

    _step1Label = new QLabel(this);
    _step1Label->setObjectName("title2Label");
    step1VBox->addWidget(_step1Label);
    step1VBox->addSpacing(boxHSpacing);

    QHBoxLayout *step1HBox = new QHBoxLayout();
    step1HBox->setContentsMargins(0, 0, 0, 0);
    step1HBox->setSpacing(boxHSpacing);
    step1VBox->addLayout(step1HBox);
    step1VBox->addSpacing(stepVMargin);

    QVBoxLayout *step1LeftVBox = new QVBoxLayout();
    step1LeftVBox->setContentsMargins(0, 0, 0, 0);
    step1HBox->addLayout(step1LeftVBox);
    step1HBox->setStretchFactor(step1LeftVBox, 1);

    QLabel *step1PictureLabel = new QLabel(this);
    step1PictureLabel->setPixmap(KDC::GuiUtility::getIconWithColor(picturePath(false)).pixmap(stepPictureSize));
    step1PictureLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    step1HBox->addWidget(step1PictureLabel);

    // Step 1.1
    QHBoxLayout *step11HBox = new QHBoxLayout();
    step11HBox->setContentsMargins(0, 0, 0, 0);
    step11HBox->setSpacing(boxHSpacing);
    step1LeftVBox->addLayout(step11HBox);

    QLabel *step11IconLabel = new QLabel(this);
    step11IconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-1.svg").pixmap(stepIconSize));
    step11IconLabel->setAlignment(Qt::AlignCenter);
    step11HBox->addWidget(step11IconLabel);
    step11HBox->addSpacing(stepIconHMargin);

    QLabel *step11Label = new QLabel(this);
    step11Label->setObjectName("largeNormalTextLabel");
    if (macOs13orLater) {
        step11Label->setText(tr("Open your Mac's <b>Privacy & Security settings</b> or "
                                " <a style=\"%1\" href=\"%2\">click here</a>")
                                 .arg(CommonUtility::linkStyle, KDC::GuiUtility::clickHereLink));
    } else {
        step11Label->setText(tr("Open your Mac's <b>Security & Privacy settings</b> or "
                                " <a style=\"%1\" href=\"%2\">click here</a>")
                                 .arg(CommonUtility::linkStyle, KDC::GuiUtility::clickHereLink));
    }
    step11Label->setWordWrap(true);
    step11HBox->addWidget(step11Label);
    step11HBox->setStretchFactor(step11Label, 1);

    if (macOs13orLater) {
        // Step 1.2
        QHBoxLayout *step12HBox = new QHBoxLayout();
        step12HBox->setContentsMargins(0, 0, 0, 0);
        step12HBox->setSpacing(boxHSpacing);
        step1LeftVBox->addLayout(step12HBox);

        QLabel *step12IconLabel = new QLabel(this);
        step12IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-2.svg").pixmap(stepIconSize));
        step12IconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        step12HBox->addWidget(step12IconLabel);
        step12HBox->addSpacing(stepIconHMargin);

        QLabel *step12Label = new QLabel(this);
        step12Label->setObjectName("largeNormalTextLabel");
        step12Label->setText(tr("Go to <b>\"Security\"</b> section"));
        step12Label->setWordWrap(true);
        step12HBox->addWidget(step12Label);
        step12HBox->setStretchFactor(step12Label, 1);

        // Step 1.3
        QHBoxLayout *step13HBox = new QHBoxLayout();
        step13HBox->setContentsMargins(0, 0, 0, 0);
        step13HBox->setSpacing(boxHSpacing);
        step1LeftVBox->addLayout(step13HBox);

        QLabel *step13IconLabel = new QLabel(this);
        step13IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-3.svg").pixmap(stepIconSize));
        step13IconLabel->setAlignment(Qt::AlignCenter);
        step13HBox->addWidget(step13IconLabel);
        step13HBox->addSpacing(stepIconHMargin);

        QLabel *step13Label = new QLabel(this);
        step13Label->setObjectName("largeNormalTextLabel");
        step13Label->setText(tr("Authorize the kDrive application in the box indicating that kDrive has been blocked"));
        step13Label->setWordWrap(true);
        step13HBox->addWidget(step13Label);
        step13HBox->setStretchFactor(step13Label, 1);
    } else {
        // Step 1.2
        QHBoxLayout *step12HBox = new QHBoxLayout();
        step12HBox->setContentsMargins(0, 0, 0, 0);
        step12HBox->setSpacing(boxHSpacing);
        step1LeftVBox->addLayout(step12HBox);

        QLabel *step12IconLabel = new QLabel(this);
        step12IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-2.svg").pixmap(stepIconSize));
        step12IconLabel->setAlignment(Qt::AlignCenter);
        step12HBox->addWidget(step12IconLabel);
        step12HBox->addSpacing(stepIconHMargin);

        QLabel *step12Label = new QLabel(this);
        step12Label->setObjectName("largeNormalTextLabel");
        step12Label->setText(tr(
            "Unlock the padlock <img src=\":/client/resources/icons/actions/lock.png\"> and authorize the kDrive application"));
        step12Label->setWordWrap(true);
        step12HBox->addWidget(step12Label);
        step12HBox->setStretchFactor(step12Label, 1);
    }

    // Step 2
    QVBoxLayout *step2VBox = new QVBoxLayout();
    step2VBox->setContentsMargins(0, 0, 0, 0);
    _mainLayout->addLayout(step2VBox);

    _step2Label = new QLabel(this);
    _step2Label->setObjectName("title2Label");
    step2VBox->addWidget(_step2Label);
    step2VBox->addSpacing(boxHSpacing);

    QHBoxLayout *step2HBox = new QHBoxLayout();
    step2HBox->setContentsMargins(0, 0, 0, 0);
    step2HBox->setSpacing(boxHSpacing);
    step2VBox->addLayout(step2HBox);
    _mainLayout->addStretch();

    QVBoxLayout *step2LeftVBox = new QVBoxLayout();
    step2LeftVBox->setContentsMargins(0, 0, 0, 0);
    step2HBox->addLayout(step2LeftVBox);
    step2HBox->setStretchFactor(step2LeftVBox, 1);

    QLabel *step2PictureLabel = new QLabel(this);
    step2PictureLabel->setPixmap(KDC::GuiUtility::getIconWithColor(picturePath(true)).pixmap(stepPictureSize));
    step2PictureLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    step2HBox->addWidget(step2PictureLabel);

    // Step 2.1
    QHBoxLayout *step21HBox = new QHBoxLayout();
    step21HBox->setContentsMargins(0, 0, 0, 0);
    step21HBox->setSpacing(boxHSpacing);
    step2LeftVBox->addLayout(step21HBox);

    QLabel *step21IconLabel = new QLabel(this);
    step21IconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-1.svg").pixmap(stepIconSize));
    step21IconLabel->setAlignment(Qt::AlignCenter);
    step21HBox->addWidget(step21IconLabel);
    step21HBox->addSpacing(stepIconHMargin);

    QLabel *step21Label = new QLabel(this);
    step21Label->setObjectName("largeNormalTextLabel");
    if (macOs13orLater) {
        step21Label->setText(tr("Go to <b>\"Privacy\"</b> section and click on <b>\"Full Disk Access\"</b> or"
                                " <a style=\"%1\" href=\"%2\">click here</a>")
                                 .arg(CommonUtility::linkStyle, KDC::GuiUtility::clickHereLink2));
    } else {
        step21Label->setText(tr("Still in the Security & Privacy settings, open the <b>\"Privacy\"</b> tab or"
                                " <a style=\"%1\" href=\"%2\">click here</a>")
                                 .arg(CommonUtility::linkStyle, KDC::GuiUtility::clickHereLink2));
    }
    step21Label->setWordWrap(true);
    step21HBox->addWidget(step21Label);
    step21HBox->setStretchFactor(step21Label, 1);

    if (macOs13orLater) {
        // Step 2.2
        QHBoxLayout *step22HBox = new QHBoxLayout();
        step22HBox->setContentsMargins(0, 0, 0, 0);
        step22HBox->setSpacing(boxHSpacing);
        step2LeftVBox->addLayout(step22HBox);

        QLabel *step22IconLabel = new QLabel(this);
        step22IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-2.svg").pixmap(stepIconSize));
        step22IconLabel->setAlignment(Qt::AlignCenter);
        step22HBox->addWidget(step22IconLabel);
        step22HBox->addSpacing(stepIconHMargin);

        QLabel *step22Label = new QLabel(this);
        step22Label->setObjectName("largeNormalTextLabel");
        step22Label->setText(
            tr("Check the \"kDrive LiteSync Extension\" box then the \"kDrive.app\" box (if not already checked)"));
        step22Label->setWordWrap(true);
        step22HBox->addWidget(step22Label);
        step22HBox->setStretchFactor(step22Label, 1);

        // Step 2.3
        QHBoxLayout *step23HBox = new QHBoxLayout();
        step23HBox->setContentsMargins(0, 0, 0, 0);
        step23HBox->setSpacing(boxHSpacing);
        step2LeftVBox->addLayout(step23HBox);

        QLabel *step23IconLabel = new QLabel(this);
        step23IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-3.svg").pixmap(stepIconSize));
        step23IconLabel->setAlignment(Qt::AlignCenter);
        step23HBox->addWidget(step23IconLabel);
        step23HBox->addSpacing(stepIconHMargin);

        QLabel *step23Label = new QLabel(this);
        step23Label->setObjectName("largeNormalTextLabel");
        step23Label->setText(tr("A restart of the app might be proposed, in this case accept it"));
        step23Label->setWordWrap(true);
        step23HBox->addWidget(step23Label);
        step23HBox->setStretchFactor(step23Label, 1);
    } else {
        // Step 2.2
        QHBoxLayout *step22HBox = new QHBoxLayout();
        step22HBox->setContentsMargins(0, 0, 0, 0);
        step22HBox->setSpacing(boxHSpacing);
        step2LeftVBox->addLayout(step22HBox);

        QLabel *step22IconLabel = new QLabel(this);
        step22IconLabel->setPixmap(
            KDC::GuiUtility::getIconWithColor(":/client/resources/icons/actions/step-2.svg").pixmap(stepIconSize));
        step22IconLabel->setAlignment(Qt::AlignCenter);
        step22HBox->addWidget(step22IconLabel);
        step22HBox->addSpacing(stepIconHMargin);

        QLabel *step22Label = new QLabel(this);
        step22Label->setObjectName("largeNormalTextLabel");
        step22Label->setText(
            tr("Check the \"kDrive LiteSync Extension\" box (and \"kDrive.app\" if it exists) in <b>\"Full Disk Access\"</b>"));
        step22Label->setWordWrap(true);
        step22HBox->addWidget(step22Label);
        step22HBox->setStretchFactor(step22Label, 1);
    }

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(0, 0, 0, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    _mainLayout->addLayout(buttonsHBox);

    buttonsHBox->addStretch();

    _finishedButton = new QPushButton(this);
    _finishedButton->setObjectName("defaultbutton");
    _finishedButton->setFlat(true);
    buttonsHBox->addWidget(_finishedButton);

    connect(step11Label, &QLabel::linkActivated, this, &CustomExtensionSetupWidget::onLinkActivated);
    connect(step21Label, &QLabel::linkActivated, this, &CustomExtensionSetupWidget::onLinkActivated);
    connect(_finishedButton, &QPushButton::clicked, this, &CustomExtensionSetupWidget::onFinishedButtonTriggered);
}

QString CustomExtensionSetupWidget::picturePath(bool fullDiskAccess) {
    bool macOs13orLater = QSysInfo::productVersion().toDouble() >= 13.0;
    bool useFrench = ParametersCache::instance()->parametersInfo().language() == LanguageFrench;

    if (fullDiskAccess) {
        if (macOs13orLater) {
            if (useFrench) {
                return ":/client/resources/pictures/fr-full-disk-access.svg";
            } else {
                return ":/client/resources/pictures/en-full-disk-access.svg";
            }
        } else {
            return ":/client/resources/pictures/full-access-disk.svg";
        }
    } else {
        if (macOs13orLater) {
            if (useFrench) {
                return ":/client/resources/pictures/fr-security.svg";
            } else {
                return ":/client/resources/pictures/en-security.svg";
            }
        } else {
            return ":/client/resources/pictures/lock-allow-kdrive.svg";
        }
    }
}

void CustomExtensionSetupWidget::onLinkActivated(const QString &link) {
    if (link == KDC::GuiUtility::clickHereLink) {
        QString cmd = QString("open \"x-apple.systempreferences:com.apple.preference.security?Security\"");
        int status = system(cmd.toLocal8Bit());
        if (status != 0) {
            qCWarning(lcCustomExtensionSetupWidget()) << "Cannot open System Preferences window!";
        }
    } else if (link == KDC::GuiUtility::clickHereLink2) {
        QString cmd = QString("open \"x-apple.systempreferences:com.apple.preference.security?Privacy_AllFiles\"");
        int status = system(cmd.toLocal8Bit());
        if (status != 0) {
            qCWarning(lcCustomExtensionSetupWidget()) << "Cannot open System Preferences window!";
        }
    }
}

void CustomExtensionSetupWidget::setLogoColor(const QColor &color) {
    _logoColor = color;
    _logoTextIconLabel->setPixmap(
        KDC::GuiUtility::getIconWithColor(":/client/resources/logos/kdrive-text-only.svg", _logoColor).pixmap(logoTextIconSize));
}

void CustomExtensionSetupWidget::onFinishedButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    emit finishedButtonTriggered();
}

void CustomExtensionSetupWidget::onUpdateProgress() {
    int progress = 0;

#ifdef Q_OS_MAC
    // Check LiteSync ext authorizations
    std::string liteSyncExtErrorDescr;
    bool liteSyncExtStep1Ok = CommonUtility::isLiteSyncExtEnabled();
    bool liteSyncExtStep2Ok = CommonUtility::isLiteSyncExtFullDiskAccessAuthOk(liteSyncExtErrorDescr);
    if (!liteSyncExtErrorDescr.empty()) {
        qCWarning(lcCustomExtensionSetupWidget)
            << "Error in CommonUtility::isLiteSyncExtFullDiskAccessAuthOk: " << liteSyncExtErrorDescr.c_str();
    }

    if (liteSyncExtStep1Ok) {
        progress = 1;

        if (liteSyncExtStep2Ok) {
            progress = 2;
        }
    }
#endif

    _step1Label->setText(QString("%1 %2").arg(tr("STEP 1"), progress > 0 ? tr("(Done)") : QString()));
    _step2Label->setText(QString("%1 %2").arg(tr("STEP 2"), progress > 1 ? tr("(Done)") : QString()));

    if (progress < 2) {
        _finishedButton->setText(QString("%1/2 %2").arg(progress).arg(tr("STEPS PERFORMED")));
    } else {
        _finishedButton->setText(tr("END"));
    }

    _finishedButton->setEnabled(progress == 2);
    _timer->start(1000);
}


}  // namespace KDC
