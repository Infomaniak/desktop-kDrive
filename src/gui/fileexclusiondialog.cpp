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

#include "fileexclusiondialog.h"
#include "fileexclusionnamedialog.h"
#include "custompushbutton.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "enablestateholder.h"
#include "appclient.h"
#include "parameterscache.h"
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
static const int hiddenFilesBoxVMargin = 15;
static const int addFileBoxHMargin = 35;
static const int addFileBoxVMargin = 12;
static const int filesTableHeaderBoxVMargin = 15;
static const int filesTableBoxVMargin = 20;

static const int rowHeight = 38;

static QVector<int> tableColumnWidth = QVector<int>() << 255  // tableColumn::Pattern
                                                      << 190  // tableColumn::Deletable
                                                      << 35;  // tableColumn::Action

static const int viewIconPathRole = Qt::UserRole;
static const int readOnlyRole = Qt::UserRole + 1;

static const char patternProperty[] = "pattern";

Q_LOGGING_CATEGORY(lcFileExclusionDialog, "gui.fileexclusiondialog", QtInfoMsg)

FileExclusionDialog::FileExclusionDialog(QWidget *parent)
    : CustomDialog(true, parent),
      _hiddenFilesCheckBox(nullptr),
      _filesTableModel(nullptr),
      _filesTableView(nullptr),
      _saveButton(nullptr),
      _actionIconColor(QColor()),
      _actionIconSize(QSize()),
      _needToSave(false),
      _defaultTemplateList(QList<ExclusionTemplateInfo>()),
      _userTemplateList(QList<ExclusionTemplateInfo>()) {
    initUI();
    updateUI();
}

void FileExclusionDialog::initUI() {
    setObjectName("FileExclusionDialog");

    QVBoxLayout *mainLayout = this->mainLayout();

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    titleLabel->setText(tr("Excluded files"));
    mainLayout->addWidget(titleLabel);
    mainLayout->addSpacing(titleBoxVMargin);

    // Description
    QLabel *descriptionLabel = new QLabel(this);
    descriptionLabel->setObjectName("descriptionLabel");
    descriptionLabel->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    descriptionLabel->setText(tr("Add files or folders that will not be synchronized on your computer."));
    descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addSpacing(descriptionBoxVMargin);

    // Synchronize hidden files
    QHBoxLayout *hiddenFilesHBox = new QHBoxLayout();
    hiddenFilesHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(hiddenFilesHBox);
    mainLayout->addSpacing(hiddenFilesBoxVMargin);

    _hiddenFilesCheckBox = new CustomCheckBox(this);
    _hiddenFilesCheckBox->setObjectName("hiddenFilesCheckBox");
    _hiddenFilesCheckBox->setText(tr("Synchronize hidden files"));
    hiddenFilesHBox->addWidget(_hiddenFilesCheckBox);

    // Add file button
    QHBoxLayout *addFileHBox = new QHBoxLayout();
    addFileHBox->setContentsMargins(addFileBoxHMargin, 0, addFileBoxHMargin, 0);
    mainLayout->addLayout(addFileHBox);
    mainLayout->addSpacing(addFileBoxVMargin);

    CustomPushButton *addFileButton = new CustomPushButton(":/client/resources/icons/actions/add.svg", tr("Add"), this);
    addFileButton->setObjectName("addButton");
    addFileHBox->addWidget(addFileButton);
    addFileHBox->addStretch();

    // Files table header
    QHBoxLayout *filesTableHeaderHBox = new QHBoxLayout();
    filesTableHeaderHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    filesTableHeaderHBox->setSpacing(0);
    mainLayout->addLayout(filesTableHeaderHBox);
    mainLayout->addSpacing(filesTableHeaderBoxVMargin);

    QLabel *header1Label = new QLabel(tr("NAME"), this);
    header1Label->setObjectName("header");
    header1Label->setFixedWidth(tableColumnWidth[tableColumn::Pattern]);
    filesTableHeaderHBox->addWidget(header1Label);

    QLabel *header2Label = new QLabel(tr("WARNING"), this);
    header2Label->setObjectName("header");
    header2Label->setFixedWidth(tableColumnWidth[tableColumn::Warning]);
    header2Label->setAlignment(Qt::AlignCenter);
    filesTableHeaderHBox->addWidget(header2Label);
    filesTableHeaderHBox->addStretch();

    // Files table view
    QHBoxLayout *filesTableHBox = new QHBoxLayout();
    filesTableHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    mainLayout->addLayout(filesTableHBox);
    mainLayout->addSpacing(filesTableBoxVMargin);

    _filesTableModel = new QStandardItemModel();
    _filesTableView = new QTableView(this);
    _filesTableView->setModel(_filesTableModel);
    _filesTableView->horizontalHeader()->hide();
    _filesTableView->verticalHeader()->hide();
    _filesTableView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    _filesTableView->verticalHeader()->setDefaultSectionSize(rowHeight);
    _filesTableView->setSelectionMode(QAbstractItemView::NoSelection);
    _filesTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _filesTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    filesTableHBox->addWidget(_filesTableView);
    mainLayout->setStretchFactor(_filesTableView, 1);

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

    connect(_hiddenFilesCheckBox, &CustomCheckBox::clicked, this, &FileExclusionDialog::onHiddenFilesCheckBoxClicked);
    connect(addFileButton, &CustomPushButton::clicked, this, &FileExclusionDialog::onAddFileButtonTriggered);
    connect(_filesTableView, &QTableView::clicked, this, &FileExclusionDialog::onTableViewClicked);
    connect(_saveButton, &QPushButton::clicked, this, &FileExclusionDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &FileExclusionDialog::onExit);
    connect(this, &CustomDialog::exit, this, &FileExclusionDialog::onExit);
}

void FileExclusionDialog::updateUI() {
    _hiddenFilesCheckBox->setChecked(ParametersCache::instance()->parametersInfo().syncHiddenFiles());

    ExitCode exitCode;
    exitCode = GuiRequests::getExclusionTemplateList(true, _defaultTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::getExclusionTemplateList : " << enumClassToInt(exitCode);
        return;
    }

    exitCode = GuiRequests::getExclusionTemplateList(false, _userTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::getExclusionTemplateList : " << enumClassToInt(exitCode);
        return;
    }

    loadPatternTable();

    ClientGui::restoreGeometry(this);
    setResizable(true);
}

void FileExclusionDialog::addTemplate(const ExclusionTemplateInfo &templateInfo, bool readOnly, int &row, QString scrollToTempl,
                                      int &scrollToRow) {
    QStandardItem *patternItem = new QStandardItem(templateInfo.templ());
    QStandardItem *warningItem = new QStandardItem();
    QStandardItem *actionItem = new QStandardItem();

    QList<QStandardItem *> itemList;
    itemList.insert(tableColumn::Pattern, patternItem);
    itemList.insert(tableColumn::Warning, warningItem);
    itemList.insert(tableColumn::Action, actionItem);
    _filesTableModel->appendRow(itemList);

    patternItem->setData(readOnly, readOnlyRole);

    if (readOnly) {
        warningItem->setFlags(warningItem->flags() ^ Qt::ItemIsEnabled);
        actionItem->setFlags(actionItem->flags() ^ Qt::ItemIsEnabled);
        setActionIcon(actionItem, QString());
    } else {
        // Set custom checkbox for Warning column
        QWidget *warningWidget = new QWidget(this);
        QHBoxLayout *noWarningHBox = new QHBoxLayout();
        warningWidget->setLayout(noWarningHBox);
        noWarningHBox->setContentsMargins(0, 0, 0, 0);
        noWarningHBox->setAlignment(Qt::AlignCenter);
        CustomCheckBox *warningCheckBox = new CustomCheckBox(this);
        warningCheckBox->setChecked(templateInfo.warning());
        warningCheckBox->setAutoFillBackground(true);
        warningCheckBox->setProperty(patternProperty, templateInfo.templ());
        noWarningHBox->addWidget(warningCheckBox);
        int rowNum = _filesTableModel->rowCount() - 1;
        _filesTableView->setIndexWidget(_filesTableModel->index(rowNum, tableColumn::Warning), warningWidget);

        setActionIcon(actionItem, ":/client/resources/icons/actions/delete.svg");

        connect(warningCheckBox, &CustomCheckBox::clicked, this, &FileExclusionDialog::onWarningCheckBoxClicked);
    }

    row++;
    if (!scrollToTempl.isEmpty() && templateInfo.templ() == scrollToTempl) {
        scrollToRow = row;
    }
}

void FileExclusionDialog::setActionIconColor(const QColor &color) {
    _actionIconColor = color;
    setActionIcon();
}

void FileExclusionDialog::setActionIconSize(const QSize &size) {
    _actionIconSize = size;
    setActionIcon();
}

void FileExclusionDialog::setActionIcon() {
    if (_actionIconColor != QColor() && _actionIconSize != QSize()) {
        for (int row = 0; row < _filesTableModel->rowCount(); row++) {
            QStandardItem *item = _filesTableModel->item(row, tableColumn::Action);
            QVariant viewIconPathV = item->data(viewIconPathRole);
            if (!viewIconPathV.isNull()) {
                QString viewIconPath = qvariant_cast<QString>(viewIconPathV);
                setActionIcon(item, viewIconPath);
            }
        }
    }
}

void FileExclusionDialog::setActionIcon(QStandardItem *item, const QString &viewIconPath) {
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

void FileExclusionDialog::setNeedToSave(bool value) {
    _needToSave = value;
    _saveButton->setEnabled(value);
}

void FileExclusionDialog::loadPatternTable(QString scrollToPattern) {
    int row = -1;
    int scrollToRow = 0;

    _filesTableModel->clear();

    // Default patterns
    for (const auto &templ : _defaultTemplateList) {
        if (!templ.deleted()) {
            addTemplate(templ, true, row, scrollToPattern, scrollToRow);
        }
    }

    // User patterns
    for (const auto &templ : _userTemplateList) {
        addTemplate(templ, false, row, scrollToPattern, scrollToRow);
    }

    if (scrollToRow) {
        QModelIndex index = _filesTableModel->index(scrollToRow, tableColumn::Pattern);
        _filesTableView->scrollTo(index);
    }

    // Set pattern table columns size
    for (int i = 0; i < tableColumnWidth.count(); i++) {
        _filesTableView->setColumnWidth(i, tableColumnWidth[i]);
    }

    _filesTableView->repaint();
}

void FileExclusionDialog::onExit() {
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

void FileExclusionDialog::onHiddenFilesCheckBoxClicked(bool checked) {
    Q_UNUSED(checked)

    setNeedToSave(true);
}

void FileExclusionDialog::onAddFileButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    EnableStateHolder _(this);

    FileExclusionNameDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString templ = dialog.templ();
        for (const auto &templInfo : _userTemplateList) {
            if (templInfo.templ() == templ) {
                CustomMessageBox msgBox(QMessageBox::Information, tr("Exclusion template already exists!"), QMessageBox::Ok,
                                        this);
                msgBox.exec();
                return;
            }
        }

        _userTemplateList.append(ExclusionTemplateInfo(templ));

        // Reload table
        loadPatternTable(templ);

        setNeedToSave(true);
    }
}

void FileExclusionDialog::onTableViewClicked(const QModelIndex &index) {
    if (index.isValid()) {
        QStandardItem *templateItem = _filesTableModel->item(index.row(), tableColumn::Pattern);
        bool readOnly = templateItem->data(readOnlyRole).toBool();
        if (readOnly) return;

        if (index.column() == tableColumn::Action) {
            QStandardItem *actionItem = _filesTableModel->item(index.row(), tableColumn::Action);
            if (actionItem) {
                // Delete pattern
                CustomMessageBox msgBox(QMessageBox::Question, tr("Do you really want to delete?"),
                                        QMessageBox::Yes | QMessageBox::No, this);
                msgBox.setDefaultButton(QMessageBox::No);
                int ret = msgBox.exec();
                if (ret != QDialog::Rejected) {
                    if (ret == QMessageBox::Yes) {
                        QString templ = templateItem->data(Qt::DisplayRole).toString();

                        bool done = false;
                        auto templateIt = _defaultTemplateList.begin();
                        while (templateIt != _defaultTemplateList.end()) {
                            if (templ == templateIt->templ()) {
                                templateIt->setDeleted(true);
                                done = true;
                                break;
                            }
                            templateIt++;
                        }

                        if (!done) {
                            templateIt = _userTemplateList.begin();
                            while (templateIt != _userTemplateList.end()) {
                                if (templ == templateIt->templ()) {
                                    _userTemplateList.erase(templateIt);
                                    break;
                                }
                                templateIt++;
                            }
                        }

                        // Reload table
                        loadPatternTable(templ);

                        setNeedToSave(true);
                    }
                }
            }
        }
    }
}

void FileExclusionDialog::onWarningCheckBoxClicked(bool checked) {
    CustomCheckBox *warningCheckBox = qobject_cast<CustomCheckBox *>(sender());
    if (warningCheckBox) {
        QString templ = warningCheckBox->property(patternProperty).toString();

        bool done = false;
        auto templateIt = _defaultTemplateList.begin();
        while (templateIt != _defaultTemplateList.end()) {
            if (templ == templateIt->templ()) {
                templateIt->setWarning(checked);
                done = true;
                break;
            }
            templateIt++;
        }

        if (!done) {
            templateIt = _userTemplateList.begin();
            while (templateIt != _userTemplateList.end()) {
                if (templ == templateIt->templ()) {
                    templateIt->setWarning(checked);
                    break;
                }
                templateIt++;
            }
        }

        setNeedToSave(true);
    } else {
        qCDebug(lcFileExclusionDialog()) << "Null pointer!";
    }
}

void FileExclusionDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked)

    ExitCode exitCode = GuiRequests::setExclusionTemplateList(true, _defaultTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::setExclusionTemplateList : " << enumClassToInt(exitCode);
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Cannot save changes!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    exitCode = GuiRequests::setExclusionTemplateList(false, _userTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::setExclusionTemplateList : " << enumClassToInt(exitCode);
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Cannot save changes!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    ParametersCache::instance()->parametersInfo().setSyncHiddenFiles(_hiddenFilesCheckBox->isChecked());
    ParametersCache::instance()->saveParametersInfo();

    exitCode = GuiRequests::propagateExcludeListChange();
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::propagateExcludeListChange : " << enumClassToInt(exitCode);
    }

    accept();
}

}  // namespace KDC
