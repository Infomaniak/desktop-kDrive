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

#include "bandwidthdialog.h"
#include "custommessagebox.h"
#include "enablestateholder.h"
#include "clientgui.h"
#include "parameterscache.h"

#include <QButtonGroup>
#include <QIntValidator>
#include <QLabel>
#include <QLoggingCategory>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 25;
static const int subtitleLabelVMargin = 10;
static const int noLimitButtonVMargin = 15;
static const int valueLimitButtonVMargin = 40;
static const int valueEditSize = 80;

Q_LOGGING_CATEGORY(lcBandwidthDialog, "gui.bandwidthdialog", QtInfoMsg)

BandwidthDialog::BandwidthDialog(QWidget *parent)
    : CustomDialog(true, parent),
      _useDownloadLimit(0),
      _downloadLimit(0),
      _useUploadLimit(0),
      _uploadLimit(0),
      _downloadNoLimitButton(nullptr),
      _downloadValueLimitLineEdit(nullptr),
      _uploadNoLimitButton(nullptr),
      _uploadValueLimitButton(nullptr),
      _uploadValueLimitLineEdit(nullptr),
      _saveButton(nullptr),
      _needToSave(false) {
    initUI();

    /*_useDownloadLimit = ParametersCache::instance()->parametersInfo().useDownLoadLimit();
    _downloadLimit = ParametersCache::instance()->parametersInfo().downloadLimit();
    _useUploadLimit = ParametersCache::instance()->parametersInfo().useUploadLimit();
    _uploadLimit = ParametersCache::instance()->parametersInfo().uploadLimit();*/

    ClientGui::restoreGeometry(this);
    setResizable(true);

    updateUI();
}

void BandwidthDialog::initUI() {
    setObjectName("BandwidthDialog");

    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Bandwidth"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Download bandwidth
    QLabel *downloadLabel = new QLabel(this);
    downloadLabel->setObjectName("subtitleLabel");
    downloadLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    downloadLabel->setText(tr("Download bandwidth"));
    mainLayout->addWidget(downloadLabel);
    mainLayout->addSpacing(subtitleLabelVMargin);

    QButtonGroup *downloadButtonGroup = new QButtonGroup(this);
    downloadButtonGroup->setExclusive(true);

    QHBoxLayout *downloadNoLimitHBox = new QHBoxLayout();
    downloadNoLimitHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(downloadNoLimitHBox);
    mainLayout->addSpacing(noLimitButtonVMargin);

    _downloadNoLimitButton = new CustomRadioButton(this);
    _downloadNoLimitButton->setText(tr("No limit"));
    _downloadNoLimitButton->setAttribute(Qt::WA_MacShowFocusRect, false);
    downloadNoLimitHBox->addWidget(_downloadNoLimitButton);
    downloadNoLimitHBox->addStretch();
    downloadButtonGroup->addButton(_downloadNoLimitButton);

    QHBoxLayout *downloadValueLimitHBox = new QHBoxLayout();
    downloadValueLimitHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    downloadValueLimitHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(downloadValueLimitHBox);
    mainLayout->addSpacing(valueLimitButtonVMargin);

    _downloadValueLimitButton = new CustomRadioButton(this);
    _downloadValueLimitButton->setText(tr("Limit to"));
    _downloadValueLimitButton->setAttribute(Qt::WA_MacShowFocusRect, false);
    downloadValueLimitHBox->addWidget(_downloadValueLimitButton);
    downloadButtonGroup->addButton(_downloadValueLimitButton);

    _downloadValueLimitLineEdit = new QLineEdit(this);
    _downloadValueLimitLineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    _downloadValueLimitLineEdit->setValidator(new QIntValidator(0, 999999, this));
    _downloadValueLimitLineEdit->setMinimumWidth(valueEditSize);
    _downloadValueLimitLineEdit->setMaximumWidth(valueEditSize);
    downloadValueLimitHBox->addWidget(_downloadValueLimitLineEdit);

    QLabel *downloadValueLabel = new QLabel(this);
    downloadValueLabel->setObjectName("textLabel");
    downloadValueLabel->setText(tr("KB/s"));
    downloadValueLimitHBox->addWidget(downloadValueLabel);
    downloadValueLimitHBox->addStretch();

    // Upload bandwidth
    QLabel *uploadLabel = new QLabel(this);
    uploadLabel->setObjectName("subtitleLabel");
    uploadLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    uploadLabel->setText(tr("Upload bandwidth"));
    mainLayout->addWidget(uploadLabel);
    mainLayout->addSpacing(subtitleLabelVMargin);

    QButtonGroup *uploadButtonGroup = new QButtonGroup(this);
    uploadButtonGroup->setExclusive(true);

    QHBoxLayout *uploadNoLimitHBox = new QHBoxLayout();
    uploadNoLimitHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(uploadNoLimitHBox);
    mainLayout->addSpacing(noLimitButtonVMargin);

    _uploadNoLimitButton = new CustomRadioButton(this);
    _uploadNoLimitButton->setText(tr("No limit"));
    _uploadNoLimitButton->setAttribute(Qt::WA_MacShowFocusRect, false);
    uploadNoLimitHBox->addWidget(_uploadNoLimitButton);
    uploadNoLimitHBox->addStretch();
    uploadButtonGroup->addButton(_uploadNoLimitButton);

    QHBoxLayout *uploadValueLimitHBox = new QHBoxLayout();
    uploadValueLimitHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    uploadValueLimitHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(uploadValueLimitHBox);
    mainLayout->addSpacing(valueLimitButtonVMargin);

    _uploadValueLimitButton = new CustomRadioButton(this);
    _uploadValueLimitButton->setText(tr("Limit to"));
    _uploadValueLimitButton->setAttribute(Qt::WA_MacShowFocusRect, false);
    uploadValueLimitHBox->addWidget(_uploadValueLimitButton);
    uploadButtonGroup->addButton(_uploadValueLimitButton);

    _uploadValueLimitLineEdit = new QLineEdit(this);
    _uploadValueLimitLineEdit->setAttribute(Qt::WA_MacShowFocusRect, false);
    _uploadValueLimitLineEdit->setValidator(new QIntValidator(0, 999999, this));
    _uploadValueLimitLineEdit->setMinimumWidth(valueEditSize);
    _uploadValueLimitLineEdit->setMaximumWidth(valueEditSize);
    uploadValueLimitHBox->addWidget(_uploadValueLimitLineEdit);

    QLabel *uploadValueLabel = new QLabel(this);
    uploadValueLabel->setObjectName("textLabel");
    uploadValueLabel->setText(tr("KB/s"));
    uploadValueLimitHBox->addWidget(uploadValueLabel);
    uploadValueLimitHBox->addStretch();

    mainLayout->addStretch();

    // Add dialog buttons
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _saveButton = new QPushButton(this);
    _saveButton->setObjectName("defaultbutton");
    _saveButton->setFlat(true);
    _saveButton->setText(tr("SAVE"));
    _saveButton->setEnabled(false);
    buttonsHBox->addWidget(_saveButton);

    QPushButton *cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(_saveButton, &QPushButton::clicked, this, &BandwidthDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &BandwidthDialog::onExit);
    connect(this, &CustomDialog::exit, this, &BandwidthDialog::onExit);
    connect(_downloadNoLimitButton, &CustomRadioButton::toggled, this, &BandwidthDialog::onDownloadNoLimitButtonToggled);
    connect(_downloadValueLimitButton, &CustomRadioButton::toggled, this, &BandwidthDialog::onDownloadValueLimitButtonToggled);
    connect(_downloadValueLimitLineEdit, &QLineEdit::textEdited, this, &BandwidthDialog::onDownloadValueLimitTextEdited);
    connect(_uploadNoLimitButton, &CustomRadioButton::toggled, this, &BandwidthDialog::onUploadNoLimitButtonToggled);
    connect(_uploadValueLimitButton, &CustomRadioButton::toggled, this, &BandwidthDialog::onUploadValueLimitButtonToggled);
    connect(_uploadValueLimitLineEdit, &QLineEdit::textEdited, this, &BandwidthDialog::onUploadValueLimitTextEdited);
}

void BandwidthDialog::updateUI() {
    if (_useDownloadLimit <= LimitType::NoLimit) {
        if (!_downloadNoLimitButton->isChecked()) {
            _downloadNoLimitButton->setChecked(true);
        }
    } else if (_useDownloadLimit == LimitType::Limit) {
        if (!_downloadValueLimitButton->isChecked()) {
            _downloadValueLimitButton->setChecked(true);
        }
    } else {
        qCDebug(lcBandwidthDialog) << "Invalid download limit type: " << _useDownloadLimit;
        Q_ASSERT(false);
    }

    bool downloadLimit = _downloadValueLimitButton->isChecked();
    _downloadValueLimitLineEdit->setText(downloadLimit ? QString::number(_downloadLimit) : QString());
    _downloadValueLimitLineEdit->setEnabled(downloadLimit);

    if (_useUploadLimit <= LimitType::NoLimit) {
        if (!_uploadNoLimitButton->isChecked()) {
            _uploadNoLimitButton->setChecked(true);
        }
    } else if (_useUploadLimit == LimitType::Limit) {
        if (!_uploadValueLimitButton->isChecked()) {
            _uploadValueLimitButton->setChecked(true);
        }
    } else {
        qCDebug(lcBandwidthDialog) << "Invalid upload limit type: " << _useUploadLimit;
        Q_ASSERT(false);
    }

    bool uploadLimit = _uploadValueLimitButton->isChecked();
    _uploadValueLimitLineEdit->setText(uploadLimit ? QString::number(_uploadLimit) : QString());
    _uploadValueLimitLineEdit->setEnabled(uploadLimit);
}

void BandwidthDialog::setNeedToSave(bool value) {
    _needToSave = value;
    _saveButton->setEnabled(value);
}

void BandwidthDialog::onExit() {
    EnableStateHolder _(this);

    if (_needToSave) {
        CustomMessageBox msgBox(QMessageBox::Question, tr("Do you want to save your modifications?"),
                                QMessageBox::Yes | QMessageBox::No, this);
        msgBox.setDefaultButton(QMessageBox::Yes);
        int ret = msgBox.exec();
        if (ret != QDialog::Rejected) {
            if (ret == QMessageBox::Yes) {
                onSaveButtonTriggered();
            } else {
                reject();
            }
        }
    } else {
        reject();
    }
}

void BandwidthDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked);

    accept();
}

void BandwidthDialog::onDownloadNoLimitButtonToggled(bool checked) {
    if (checked && _useDownloadLimit != LimitType::NoLimit) {
        _useDownloadLimit = LimitType::NoLimit;
        updateUI();
        setNeedToSave(true);
    }
}

void BandwidthDialog::onDownloadValueLimitButtonToggled(bool checked) {
    if (checked && _useDownloadLimit != LimitType::Limit) {
        _useDownloadLimit = LimitType::Limit;
        updateUI();
        setNeedToSave(true);
    }
}

void BandwidthDialog::onDownloadValueLimitTextEdited(const QString &text) {
    _downloadLimit = text.toInt();
    setNeedToSave(true);
}

void BandwidthDialog::onUploadNoLimitButtonToggled(bool checked) {
    if (checked && _useUploadLimit != LimitType::NoLimit) {
        _useUploadLimit = LimitType::NoLimit;
        updateUI();
        setNeedToSave(true);
    }
}

void BandwidthDialog::onUploadValueLimitButtonToggled(bool checked) {
    if (checked && _useUploadLimit != LimitType::Limit) {
        _useUploadLimit = LimitType::Limit;
        updateUI();
        setNeedToSave(true);
    }
}

void BandwidthDialog::onUploadValueLimitTextEdited(const QString &text) {
    _uploadLimit = text.toInt();
    setNeedToSave(true);
}

}  // namespace KDC
