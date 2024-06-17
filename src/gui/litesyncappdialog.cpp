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
#include "litesyncappdialog.h"
#include "customproxystyle.h"
#include "guirequests.h"

#include <QBoxLayout>
#include <QLabel>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int appIdVMargin = 2;
static const int subtitleLabelVMargin = 10;

Q_LOGGING_CATEGORY(lcLiteSyncAppDialog, "gui.litesyncappdialog", QtInfoMsg)

LiteSyncAppDialog::LiteSyncAppDialog(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : CustomDialog(true, parent), _gui(gui), _appIdLineEdit(nullptr), _appNameLineEdit(nullptr), _validateButton(nullptr) {
    QVBoxLayout *mainLayout = this->mainLayout();

    QLabel *appIdLabel = new QLabel(this);
    appIdLabel->setObjectName("boldTextLabel");
    appIdLabel->setContentsMargins(boxHMargin, appIdVMargin, boxHMargin, 0);
    appIdLabel->setText(tr("Application Id"));
    mainLayout->addWidget(appIdLabel);
    mainLayout->addSpacing(subtitleLabelVMargin);

    QHBoxLayout *appIdHBox = new QHBoxLayout();
    appIdHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    appIdHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(appIdHBox);
    mainLayout->addSpacing(subtitleLabelVMargin);

#ifdef Q_OS_MAC
    // Get app fetching list from the LiteSync extension
    for (const auto &syncInfoMapElt : _gui->syncInfoMap()) {
        if (syncInfoMapElt.second.virtualFileMode() == VirtualFileModeMac) {
            try {
                ExitCode exitCode = GuiRequests::getFetchingAppList(_appTable);
                if (exitCode != ExitCode::Ok) {
                    qCWarning(lcLiteSyncAppDialog()) << "Error in Requests::getFetchingAppList : " << enumClassToInt(exitCode);
                }

                break;
            } catch (std::exception const &) {
                // Nothing to do
            }
        }
    }

    // App id
    _appIdComboBox = new CustomComboBox(this);
    _appIdComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    _appIdComboBox->setAttribute(Qt::WA_MacShowFocusRect, false);

    QHashIterator<QString, QString> appTableIt(_appTable);
    while (appTableIt.hasNext()) {
        appTableIt.next();
        _appIdComboBox->addItem(appTableIt.key());
    }

    appIdHBox->addWidget(_appIdComboBox);
#else
    _appIdLineEdit = new QLineEdit(this);
    _appIdLineEdit->setProperty(CustomProxyStyle::focusRectangleProperty, true);
    _appIdLineEdit->setFocus();
    appIdHBox->addWidget(_appIdLineEdit);
#endif

    // App name
    QLabel *appNameLabel = new QLabel(this);
    appNameLabel->setObjectName("boldTextLabel");
    appNameLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    appNameLabel->setText(tr("Application Name"));
    mainLayout->addWidget(appNameLabel);
    mainLayout->addSpacing(subtitleLabelVMargin);

    QHBoxLayout *appNameHBox = new QHBoxLayout();
    appNameHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    appNameHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(appNameHBox);
    mainLayout->addStretch();

    _appNameLineEdit = new QLineEdit(this);
    _appNameLineEdit->setProperty(CustomProxyStyle::focusRectangleProperty, true);
    appNameHBox->addWidget(_appNameLineEdit);

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _validateButton = new QPushButton(this);
    _validateButton->setObjectName("defaultbutton");
    _validateButton->setFlat(true);
    _validateButton->setText(tr("VALIDATE"));
    _validateButton->setEnabled(false);
    buttonsHBox->addWidget(_validateButton);

    QPushButton *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(_appIdComboBox, QOverload<int>::of(&QComboBox::activated), this, &LiteSyncAppDialog::onComboBoxActivated);
    connect(_appIdLineEdit, &QLineEdit::textEdited, this, &LiteSyncAppDialog::onTextEdited);
    connect(_validateButton, &QPushButton::clicked, this, &LiteSyncAppDialog::onValidateButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &LiteSyncAppDialog::onExit);
    connect(this, &CustomDialog::exit, this, &LiteSyncAppDialog::onExit);

    onComboBoxActivated(_appIdComboBox->currentIndex());
}

void LiteSyncAppDialog::appInfo(QString &appId, QString &appName) {
#ifdef Q_OS_MAC
    appId = _appIdComboBox->currentText();
#else
    appId = _appIdLineEdit->text();
#endif

    appName = _appNameLineEdit->text();
}

void LiteSyncAppDialog::onExit() {
    reject();
}

void LiteSyncAppDialog::onComboBoxActivated(int index) {
    _appNameLineEdit->setText(_appTable[_appIdComboBox->itemText(index)]);
    _validateButton->setEnabled(!_appIdComboBox->currentText().isEmpty());
}

void LiteSyncAppDialog::onTextEdited(const QString &text) {
    _validateButton->setEnabled(!text.isEmpty());
}

void LiteSyncAppDialog::onValidateButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    accept();
}

}  // namespace KDC
