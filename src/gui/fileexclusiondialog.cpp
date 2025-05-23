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

#include "fileexclusiondialog.h"
#include "fileexclusionnamedialog.h"
#include "custompushbutton.h"
#include "custommessagebox.h"
#include "guiutility.h"
#include "enablestateholder.h"
#include "appclient.h"
#include "parameterscache.h"
#include "guirequests.h"
#include "libcommongui/matomoclient.h"

#include "libcommon/utility/utility.h"

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

static QVector<int> tableColumnWidth = QVector<int>() << 255 // tableColumn::Pattern
                                                      << 190 // tableColumn::Deletable
                                                      << 35; // tableColumn::Action

static const int viewIconPathRole = Qt::UserRole;
static const int readOnlyRole = Qt::UserRole + 1;

static const char patternProperty[] = "pattern";

Q_LOGGING_CATEGORY(lcFileExclusionDialog, "gui.fileexclusiondialog", QtInfoMsg)

FileExclusionDialog::FileExclusionDialog(QWidget *parent) :
    CustomDialog(true, parent) {
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
    auto *const buttonsHBox = new QHBoxLayout();
    buttonsHBox->setContentsMargins(boxHMargin, 0, boxHMargin, 0);
    buttonsHBox->setSpacing(boxHSpacing);
    mainLayout->addLayout(buttonsHBox);

    _saveButton = new QPushButton(this);
    _saveButton->setObjectName("defaultbutton");
    _saveButton->setFlat(true);
    _saveButton->setText(tr("SAVE"));
    _saveButton->setEnabled(false);
    buttonsHBox->addWidget(_saveButton);

    auto *const cancelButton = new QPushButton(this);
    cancelButton->setObjectName("nondefaultbutton");
    cancelButton->setFlat(true);
    cancelButton->setText(tr("CANCEL"));
    buttonsHBox->addWidget(cancelButton);
    buttonsHBox->addStretch();

    connect(addFileButton, &CustomPushButton::clicked, this, &FileExclusionDialog::onAddFileButtonTriggered);
    connect(_filesTableView, &QTableView::clicked, this, &FileExclusionDialog::onTableViewClicked);
    connect(_saveButton, &QPushButton::clicked, this, &FileExclusionDialog::onSaveButtonTriggered);
    connect(cancelButton, &QPushButton::clicked, this, &FileExclusionDialog::onExit);
    connect(this, &CustomDialog::exit, this, &FileExclusionDialog::onExit);
}

namespace {

struct SyncNameHashFunction {
        using is_transparent = void; // Enables heterogeneous operations.

        std::size_t operator()(const SyncName &name) const {
            constexpr std::hash<SyncName> hashFunction;
            return hashFunction(name);
        }
};

QList<ExclusionTemplateInfo> filterOutTemplatesWrtNfcNormalization(const QList<ExclusionTemplateInfo> &templateList) {
    QList<ExclusionTemplateInfo> result;

    std::unordered_set<SyncName, SyncNameHashFunction, std::equal_to<>>
            uniqueTemplateNames; // Unique template names up to NFC-encoding.
    for (const auto &templateInfo: templateList) {
        SyncName normalizedName;
        SyncName insertedName = QStr2SyncName(templateInfo.templ());
        QString insertedString;

        if (const bool nfcSuccess = CommonUtility::normalizedSyncName(insertedName, normalizedName, UnicodeNormalization::NFC);
            !nfcSuccess) {
            qCWarning(lcFileExclusionDialog()) << "Failed to NFC-normalize the template " << templateInfo.templ();
            insertedString = templateInfo.templ();
        } else {
            insertedName = normalizedName;
            insertedString = SyncName2QStr(normalizedName);
        }

        const bool isNew = uniqueTemplateNames.emplace(insertedName).second;
        if (isNew) result.append(ExclusionTemplateInfo{insertedString});
    }

    return result;
}
} // namespace

void FileExclusionDialog::updateUI() {
    ExitCode exitCode;
    exitCode = GuiRequests::getExclusionTemplateList(true, _defaultTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::getExclusionTemplateList: code=" << exitCode;
        return;
    }

    exitCode = GuiRequests::getExclusionTemplateList(false, _userTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::getExclusionTemplateList: code=" << exitCode;
        return;
    }

    _userTemplateList = filterOutTemplatesWrtNfcNormalization(_userTemplateList);
    loadPatternTable();

    ClientGui::restoreGeometry(this);
    setResizable(true);
}

void FileExclusionDialog::addTemplate(const ExclusionTemplateInfo &templateInfo, const bool readOnly, int &row,
                                      const QString &scrollToTemplate, int &scrollToRow) {
    auto *const patternItem = new QStandardItem(templateInfo.templ());
    auto *const warningItem = new QStandardItem();
    auto *const actionItem = new QStandardItem();

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
        auto *const warningWidget = new QWidget(this);
        auto *const noWarningHBox = new QHBoxLayout();
        warningWidget->setLayout(noWarningHBox);
        noWarningHBox->setContentsMargins(0, 0, 0, 0);
        noWarningHBox->setAlignment(Qt::AlignCenter);
        auto *const warningCheckBox = new CustomCheckBox(this);
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
    if (!scrollToTemplate.isEmpty() && templateInfo.templ() == scrollToTemplate) {
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
                const auto viewIconPath = qvariant_cast<QString>(viewIconPathV);
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

void FileExclusionDialog::loadPatternTable(const QString &scrollToPattern) {
    int row = -1;
    int scrollToRow = 0;

    _filesTableModel->clear();

    // Default patterns
    for (const auto &template_: _defaultTemplateList) {
        if (!template_.deleted()) {
            addTemplate(template_, true, row, scrollToPattern, scrollToRow);
        }
    }

    // User patterns
    for (const auto &template_: _userTemplateList) {
        addTemplate(template_, false, row, scrollToPattern, scrollToRow);
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
            MatomoClient::sendEvent("preferencesFileExclusion", MatomoEventAction::Click, "exitButton",
                                    ret == QMessageBox::Yes ? 1 : 0);
        }
    } else {
        reject();
    }
}

namespace {

void logIfTemplateNormalizationFails(const SyncName &template_) {
    SyncName nfcNormalizedName;
    const bool nfcSuccess = CommonUtility::normalizedSyncName(template_, nfcNormalizedName, UnicodeNormalization::NFC);
    if (!nfcSuccess) {
        qCDebug(lcFileExclusionDialog()) << "Error in CommonUtility::normalizedSyncName. Failed to NFC-normalize the template '"
                                         << SyncName2QStr(template_) << "'.";
    }

    SyncName nfdNormalizedName;
    const bool nfdSuccess = CommonUtility::normalizedSyncName(template_, nfdNormalizedName, UnicodeNormalization::NFD);
    if (!nfdSuccess) {
        qCDebug(lcFileExclusionDialog()) << "Error in CommonUtility::normalizedSyncName. Failed to NFD-normalize the template '"
                                         << SyncName2QStr(template_) << "'.";
    }

    if (!nfcSuccess || !nfdSuccess) {
        qCWarning(lcFileExclusionDialog())
                << "File exclusion based on user templates may fail to exclude file names depending on their normalizations.";
    }
}

//
//! Computes and returns all possible NFC and NFD normalizations of `templateString` segments
//! interpreted as a file system path.
/*!
  \param templateString is the pattern string the normalizations of which are queried.
  \return a set of QString containing the NFC and NFD normalizations of exclusionTemplate, if those have been successful.
  The returned set contains additionally the string exclusionTemplate in any case.
*/
std::unordered_set<QString> computeNormalizations(const QString &templateString) {
    logIfTemplateNormalizationFails(QStr2SyncName(templateString));

    const auto normalizations = CommonUtility::computePathNormalizations(QStr2SyncName(templateString));
    std::unordered_set<QString> result;

    for (const SyncName &normalization: normalizations) (void) result.emplace(SyncName2QStr(normalization));


    return result;
}


QList<ExclusionTemplateInfo> computeNormalizations(const QList<ExclusionTemplateInfo> &templateList) {
    QList<ExclusionTemplateInfo> result;

    for (const auto &templateInfo: templateList) {
        const auto normalizations = computeNormalizations(templateInfo.templ());
        for (const auto &normalization: normalizations) result.append(ExclusionTemplateInfo{normalization});
    }

    return result;
}


bool containsStringPattern(const QList<ExclusionTemplateInfo> &exclusionList, const QString &pattern) {
    const auto predicate = [&pattern](const ExclusionTemplateInfo &eti) { return eti.templ() == pattern; };
    const auto &it = std::find_if(exclusionList.cbegin(), exclusionList.cend(), predicate);

    return (it != exclusionList.cend());
}

} // namespace


void FileExclusionDialog::onAddFileButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("preferencesFileExclusion", MatomoEventAction::Click, "addFileButton");
    EnableStateHolder _(this);

    FileExclusionNameDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString &template_ = dialog.templ();
    const auto &normalizedTemplates = computeNormalizations(template_);

    const auto predicate = [this](const auto &templateNormalizedString) {
        return containsStringPattern(_userTemplateList, templateNormalizedString) ||
               containsStringPattern(_defaultTemplateList, templateNormalizedString);
    };

    // Insert `template_` only if none of its normalizations can be found in the default and user lists.
    if (const auto it = std::find_if(normalizedTemplates.cbegin(), normalizedTemplates.cend(), predicate);
        it != normalizedTemplates.cend()) {
        CustomMessageBox msgBox(QMessageBox::Information, tr("Exclusion template already exists!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    _userTemplateList.append(template_);

    loadPatternTable(template_); // Reload and scroll to newly inserted entry.
    setNeedToSave(true);
}

void FileExclusionDialog::onTableViewClicked(const QModelIndex &index) {
    if (!index.isValid()) return;

    const QStandardItem *const templateItem = _filesTableModel->item(index.row(), tableColumn::Pattern);
    if (const bool readOnly = templateItem->data(readOnlyRole).toBool(); readOnly) return;

    if (index.column() != tableColumn::Action) return;

    if (const QStandardItem *const actionItem = _filesTableModel->item(index.row(), tableColumn::Action); !actionItem) return;

    CustomMessageBox msgBox(QMessageBox::Question, tr("Do you really want to delete?"), QMessageBox::Yes | QMessageBox::No, this);
    msgBox.setDefaultButton(QMessageBox::No);

    const int ret = msgBox.exec();
    MatomoClient::sendEvent("preferencesFileExclusion", MatomoEventAction::Click, "deleteItemButton",
                            ret == QMessageBox::Yes ? 1 : 0);

    if (ret != QMessageBox::Yes) return;

    // Delete template
    const QString &template_ = templateItem->data(Qt::DisplayRole).toString();
    for (auto templateIt = _userTemplateList.begin(); templateIt != _userTemplateList.end(); ++templateIt) {
        if (templateIt->templ() == template_) {
            _userTemplateList.erase(templateIt);
            break;
        }
    }

    loadPatternTable();
    setNeedToSave(true);
}

void FileExclusionDialog::onWarningCheckBoxClicked(bool checked) {
    const auto *const warningCheckBox = qobject_cast<CustomCheckBox *>(sender());
    MatomoClient::sendEvent("preferencesFileExclusion", MatomoEventAction::Click, "warningCheckbox", checked ? 1 : 0);

    if (!warningCheckBox) {
        qCDebug(lcFileExclusionDialog()) << "Null pointer!";
        return;
    }

    const QString &template_ = warningCheckBox->property(patternProperty).toString();
    bool done = false;

    for (auto &templateInfo: _defaultTemplateList) {
        if (template_ == templateInfo.templ()) {
            templateInfo.setWarning(checked);
            done = true;
            break;
        }
    }

    if (!done) {
        for (auto &templateInfo: _userTemplateList) {
            if (template_ == templateInfo.templ()) {
                templateInfo.setWarning(checked);
                break;
            }
        }
    }

    setNeedToSave(true);
}

void FileExclusionDialog::onSaveButtonTriggered(bool checked) {
    Q_UNUSED(checked)
    MatomoClient::sendEvent("preferencesFileExclusion", MatomoEventAction::Click, "saveButton");

    ExitCode exitCode = GuiRequests::setExclusionTemplateList(true, _defaultTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::setExclusionTemplateList: code=" << exitCode;
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Cannot save changes!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    auto expandedUserTemplateList = computeNormalizations(_userTemplateList);
    exitCode = GuiRequests::setExclusionTemplateList(false, expandedUserTemplateList);
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::setExclusionTemplateList: code=" << exitCode;
        CustomMessageBox msgBox(QMessageBox::Warning, tr("Cannot save changes!"), QMessageBox::Ok, this);
        msgBox.exec();
        return;
    }

    ParametersCache::instance()->saveParametersInfo();

    exitCode = GuiRequests::propagateExcludeListChange();
    if (exitCode != ExitCode::Ok) {
        qCWarning(lcFileExclusionDialog()) << "Error in Requests::propagateExcludeListChange: code=" << exitCode;
    }

    accept();
}

} // namespace KDC
