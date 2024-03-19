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
#include "litesyncdialog.h"
#include "litesyncappdialog.h"
#include "custompushbutton.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "enablestateholder.h"
#include "guirequests.h"

#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QStandardItem>
#include <QStringList>
#include <QVector>

namespace KDC {

static const int boxHMargin = 40;
static const int boxHSpacing = 10;
static const int titleBoxVMargin = 14;
static const int descriptionBoxVMargin = 20;
static const int addFileBoxHMargin = 35;
static const int addFileBoxVMargin = 12;
static const int filesTableHeaderBoxVMargin = 15;
static const int filesTableBoxVMargin = 20;

static const int rowHeight = 38;

static QVector<int> tableColumnWidth = QVector<int>() << 255  // tableColumn::AppId
                                                      << 190  // tableColumn::AppName
                                                      << 35;  // tableColumn::Action

static const int viewIconPathRole = Qt::UserRole;
static const int readOnlyRole = Qt::UserRole + 1;

Q_LOGGING_CATEGORY(lcLiteSyncDialog, "gui.litesyncdialog", QtInfoMsg)

LiteSyncDialog::LiteSyncDialog(std::shared_ptr<ClientGui> gui, QWidget *parent)
    : CustomDialog(true, parent),
      _gui(gui),
      _appsTableModel(nullptr),
      _appsTableView(nullptr),
      _saveButton(nullptr),
      _actionIconColor(QColor()),
      _actionIconSize(QSize()),
      _needToSave(false),
      _defaultAppList(QList<ExclusionAppInfo>()),
      _userAppList(QList<ExclusionAppInfo>()) {
    initUI();
    updateUI();
}

void LiteSyncDialog::initUI() {
    setObjectName("LiteSyncDialog");

    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText("Lite Sync");
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    QLabel *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName("descriptionLabel");
    descriptionLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    descriptionLabel->setText(
        tr("Some apps (backup, anti-virus...) access your files, which leads to their download when they are \"online\". Add "
           "them in the list below to avoid this behaviour."));
    descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addSpacing(descriptionBoxVMargin);

    // Add app button
    QHBoxLayout *addAppHBox = new QHBoxLayout();
    addAppHBox->setContentsMargins(addFileBoxHMargin, 0, addFileBoxHMargin, 0);
    mainLayout->addLayout(addAppHBox);
    mainLayout->addSpacing(addFileBoxVMargin);

    CustomPushButton *addAppButton = new CustomPushButton(":/client/resources/icons/actions/add.svg", tr("Add"), this);
    addAppButton->setObjectName("addButton");
    addAppHBox->addWidget(addAppButton);
    addAppHBox->addStretch();

    // Apps table header
    QHBoxLayout *appsTableHeaderHBox = new QHBoxLayout();
    appsTableHeaderHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    appsTableHeaderHBox->setSpacing(0);
    mainLayout->addLayout(appsTableHeaderHBox);
    mainLayout->addSpacing(filesTableHeaderBoxVMargin);

    QLabel *header1Label = new QLabel(tr("APPLICATION ID"), this);
    header1Label->setObjectName("header");
    header1Label->setFixedWidth(tableColumnWidth[tableColumn::AppId]);
    appsTableHeaderHBox->addWidget(header1Label);

    QLabel *header2Label = new QLabel(tr("NAME"), this);
    header2Label->setObjectName("header");
    header2Label->setFixedWidth(tableColumnWidth[tableColumn::AppName]);
    appsTableHeaderHBox->addWidget(header2Label);
    appsTableHeaderHBox->addStretch();

    // Apps table view
    QHBoxLayout *appsTableHBox = new QHBoxLayout();
    appsTableHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(appsTableHBox);
    mainLayout->addSpacing(filesTableBoxVMargin);

    _appsTableModel = new QStandardItemModel();
    _appsTableView = new QTableView(this);
    _appsTableView->setModel(_appsTableModel);
    _appsTableView->horizontalHeader()->hide();
    _appsTableView->verticalHeader()->hide();
    _appsTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    _appsTableView->verticalHeader()->setDefaultSectionSize(rowHeight);
    _appsTableView->setSelectionMode(QAbstractItemView::NoSelection);
    _appsTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _appsTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    appsTableHBox->addWidget(_appsTableView);
    mainLayout->setStretchFactor(_appsTableView, 1);

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

    connect(addAppButton, &CustomPushButton::clicked, this, &LiteSyncDialog::onAddAppButtonTriggered);
    connect(_appsTableView, &QTableView::clicked, this, &LiteSyncDialog::onTableViewClicked);
    connect(_saveButton, &QPushButton::clicked, this, &LiteSyncDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &LiteSyncDialog::onExit);
    connect(this, &CustomDialog::exit, this, &LiteSyncDialog::onExit);
}

void LiteSyncDialog::updateUI() {
#ifdef Q_OS_MAC
    ExitCode exitCode;
    exitCode = GuiRequests::getExclusionAppList(true, _defaultAppList);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcLiteSyncDialog()) << "Error in Requests::getExclusionAppList : " << exitCode;
        return;
    }

    exitCode = GuiRequests::getExclusionAppList(false, _userAppList);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcLiteSyncDialog()) << "Error in Requests::getExclusionAppList : " << exitCode;
        return;
    }
#endif

    loadAppTable();

    ClientGui::restoreGeometry(this);
    setResizable(true);
}

void LiteSyncDialog::addApp(const ExclusionAppInfo &appInfo, bool readOnly, int &row, QString scrollToAppId, int &scrollToRow) {
    QStandardItem *appIdItem = new QStandardItem(appInfo.appId());
    QStandardItem *appNameItem = new QStandardItem(appInfo.description());
    QStandardItem *actionItem = new QStandardItem();

    QList<QStandardItem *> itemList;
    itemList.insert(tableColumn::AppId, appIdItem);
    itemList.insert(tableColumn::AppName, appNameItem);
    itemList.insert(tableColumn::Action, actionItem);
    _appsTableModel->appendRow(itemList);

    appIdItem->setData(readOnly, readOnlyRole);
    appNameItem->setData(readOnly, readOnlyRole);

    if (readOnly) {
        actionItem->setFlags(actionItem->flags() ^ Qt::ItemIsEnabled);
        setActionIcon(actionItem, QString());
    } else {
        setActionIcon(actionItem, ":/client/resources/icons/actions/delete.svg");
    }

    row++;
    if (!scrollToAppId.isEmpty() && appInfo.appId() == scrollToAppId) {
        scrollToRow = row;
    }
}

void LiteSyncDialog::setActionIconColor(const QColor &color) {
    _actionIconColor = color;
    setActionIcon();
}

void LiteSyncDialog::setActionIconSize(const QSize &size) {
    _actionIconSize = size;
    setActionIcon();
}

void LiteSyncDialog::setActionIcon() {
    if (_actionIconColor != QColor() && _actionIconSize != QSize()) {
        for (int row = 0; row < _appsTableModel->rowCount(); row++) {
            QStandardItem *item = _appsTableModel->item(row, tableColumn::Action);
            QVariant viewIconPathV = item->data(viewIconPathRole);
            if (!viewIconPathV.isNull()) {
                QString viewIconPath = qvariant_cast<QString>(viewIconPathV);
                setActionIcon(item, viewIconPath);
            }
        }
    }
}

void LiteSyncDialog::setActionIcon(QStandardItem *item, const QString &viewIconPath) {
    if (item) {
        if (item->data(viewIconPathRole).isNull()) {
            item->setData(viewIconPath, viewIconPathRole);
        }
        if (_actionIconColor != QColor() && _actionIconSize != QSize() && !viewIconPath.isEmpty()) {
            item->setData(KDC::GuiUtility::getIconWithColor(viewIconPath, _actionIconColor).pixmap(_actionIconSize),
                          Qt::DecorationRole);
        }
    }
}

void LiteSyncDialog::setNeedToSave(bool value) {
    _needToSave = value;
    _saveButton->setEnabled(value);
}

void LiteSyncDialog::loadAppTable(QString scrollToAppId) {
    int row = -1;
    int scrollToRow = 0;

    _appsTableModel->clear();

    // Default apps
    for (const auto &app : _defaultAppList) {
        addApp(app, true, row, scrollToAppId, scrollToRow);
    }

    // User apps
    for (const auto &app : _userAppList) {
        addApp(app, false, row, scrollToAppId, scrollToRow);
    }

    if (scrollToRow) {
        QModelIndex index = _appsTableModel->index(scrollToRow, tableColumn::AppId);
        _appsTableView->scrollTo(index);
    }

    // Set app table columns size
    for (int i = 0; i < tableColumnWidth.count(); i++) {
        _appsTableView->setColumnWidth(i, tableColumnWidth[i]);
    }

    _appsTableView->repaint();
}

void LiteSyncDialog::onExit() {
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

void LiteSyncDialog::onAddAppButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    EnableStateHolder _(this);

    LiteSyncAppDialog dialog(_gui, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString appId;
        QString appName;
        dialog.appInfo(appId, appName);
        _userAppList.append(ExclusionAppInfo(appId, appName, false));

        // Reload table
        loadAppTable(appId);

        setNeedToSave(true);
    }
}

void LiteSyncDialog::onTableViewClicked(const QModelIndex &index) {
    if (index.isValid()) {
        if (index.column() == tableColumn::Action) {
            QStandardItem *item = _appsTableModel->item(index.row(), tableColumn::Action);
            if (item) {
                // Delete app
                CustomMessageBox msgBox(QMessageBox::Question, tr("Do you really want to delete?"),
                                        QMessageBox::Yes | QMessageBox::No, this);
                msgBox.setDefaultButton(QMessageBox::No);
                int ret = msgBox.exec();
                if (ret != QDialog::Rejected) {
                    if (ret == QMessageBox::Yes) {
                        QString appId = _appsTableModel->index(index.row(), tableColumn::AppId).data(Qt::DisplayRole).toString();
                        const auto appIt = _userAppList.constBegin();
                        while (appIt != _userAppList.constEnd()) {
                            if (appId == appIt->appId()) {
                                _userAppList.erase(appIt);
                                break;
                            }
                        }

                        // Reload table
                        loadAppTable(appId);

                        setNeedToSave(true);
                    }
                }
            }
        }
    }
}

void LiteSyncDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked)

#ifdef Q_OS_MAC
    ExitCode exitCode;
    exitCode = GuiRequests::setExclusionAppList(false, _userAppList);
    if (exitCode != ExitCodeOk) {
        qCWarning(lcLiteSyncDialog()) << "Error in Requests::setExclusionAppList : " << exitCode;
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Cannot save changes!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }
#endif

    accept();
}

}  // namespace KDC
