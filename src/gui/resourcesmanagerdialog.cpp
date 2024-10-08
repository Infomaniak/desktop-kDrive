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

#include "resourcesmanagerdialog.h"
#include "custommessagebox.h"
#include "enablestateholder.h"
#include "clientgui.h"
#include "parameterscache.h"

#include <QLabel>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 25;
static const int startingManagerValue = 50;

ResourcesManagerDialog::ResourcesManagerDialog(QWidget *parent) :
    CustomDialog(true, parent), _slideBarResources(nullptr), _saveButton(nullptr), _sliderValueLabel(nullptr),
    _needToSave(false) {
    initUI();

    ClientGui::restoreGeometry(this);
    setResizable(true);
}

void ResourcesManagerDialog::initUI() {
    setObjectName("ResourcesManagerDialog");

    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Resources Manager"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    QVBoxLayout *resourcesVBox = new QVBoxLayout(this);
    resourcesVBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    resourcesVBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(resourcesVBox);

    QLabel *sliderTitleLabel = new QLabel(this);
    sliderTitleLabel->setText(tr("Maximum CPU usage allowed"));
    sliderTitleLabel->setObjectName("textLabel");
    resourcesVBox->addWidget(sliderTitleLabel);

    _sliderValueLabel = new QLabel(this);
    _sliderValueLabel->setText(QString("%1 %").arg(startingManagerValue));
    _sliderValueLabel->setObjectName("titleLabel");
    resourcesVBox->addWidget(_sliderValueLabel, 0, Qt::AlignLeft);

    resourcesVBox->addSpacing(titleBoxVMargin);

    _slideBarResources = new QSlider(this);
    _slideBarResources->setOrientation(Qt::Horizontal);
    _slideBarResources->setVisible(true);
    _slideBarResources->setMinimum(1);
    _slideBarResources->setMaximum(100);
    _slideBarResources->setTracking(true);
    _slideBarResources->setValue(startingManagerValue);

    resourcesVBox->addWidget(_slideBarResources, 0);
    resourcesVBox->addStretch();

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

    int maxAllowCpuDbValue = ParametersCache::instance()->parametersInfo().maxAllowedCpu();

    _slideBarResources->setValue(maxAllowCpuDbValue);
    updateLabel(maxAllowCpuDbValue);

    connect(_slideBarResources, &QSlider::valueChanged, this, &ResourcesManagerDialog::updateLabel);
    connect(_saveButton, &QPushButton::clicked, this, &ResourcesManagerDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &ResourcesManagerDialog::onExit);
    connect(this, &CustomDialog::exit, this, &ResourcesManagerDialog::onExit);
}

void ResourcesManagerDialog::onExit() {
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

void ResourcesManagerDialog::setNeedToSave(bool value) {
    if (_slideBarResources->value() != ParametersCache::instance()->parametersInfo().maxAllowedCpu()) {
        _needToSave = value;
        _saveButton->setEnabled(value);
    }
}

void ResourcesManagerDialog::onSaveButtonTriggered() {
    ParametersCache::instance()->parametersInfo().setMaxAllowedCpu(_slideBarResources->value());
    if (!ParametersCache::instance()->saveParametersInfo()) {
        return;
    }
    accept();
}

void ResourcesManagerDialog::updateLabel(int sliderValue) {
    setNeedToSave(true);
    _sliderValueLabel->setText(QString::number(sliderValue) + " %");
}

} // namespace KDC
