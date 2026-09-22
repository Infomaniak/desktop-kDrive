/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "fileexclusioncontroller.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QPointer>

#include <algorithm>
#include <cstddef>

namespace KDC {

namespace {
constexpr auto loadErrorTextId = QT_TRID_NOOP("defaultErrorTitle");
constexpr auto saveErrorTextId = QT_TRID_NOOP("linuxSettingsSaveError");

QString templatePattern(const ExclusionTemplate &exclusionTemplate) {
    const auto &pattern = exclusionTemplate.templ();
    return QString::fromUtf8(pattern.data(), static_cast<qsizetype>(pattern.size()));
}

std::string utf8String(const QString &value) {
    const QByteArray utf8 = value.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

QString normalizedKey(const QString &pattern) {
    return pattern.normalized(QString::NormalizationForm_C);
}
} // namespace

FileExclusionController::FileExclusionController(ExclusionTemplateService &service, QObject *const parent) :
    QObject(parent),
    _service(service),
    _defaultRules(false, this),
    _userRules(true, this) {
    (void) connect(&_service, &ExclusionTemplateService::snapshotsChanged, this, &FileExclusionController::syncModels);
    (void) connect(&_userRules, &ExclusionRuleModel::selectionChanged, this, &FileExclusionController::changed);
    (void) connect(&_userRules, &ExclusionRuleModel::countChanged, this, &FileExclusionController::changed);
}

int FileExclusionController::selectionCheckState() const {
    if (selectedCount() == 0) {
        return Qt::Unchecked;
    }
    return selectedCount() == userRuleCount() ? Qt::Checked : Qt::PartiallyChecked;
}

QString FileExclusionController::errorTextId() const {
    switch (_error) {
        case Error::Load:
            return QString::fromLatin1(loadErrorTextId);
        case Error::Save:
            return QString::fromLatin1(saveErrorTextId);
        case Error::None:
            return {};
    }

    return {};
}

void FileExclusionController::refresh() {
    if (_loading || _saving) {
        return;
    }

    _loading = true;
    _error = Error::None;
    emit changed();

    _service.refresh([self = QPointer(this)](const ExitInfo &result) {
        if (!self) {
            return;
        }

        self->_loading = false;
        self->_error = result ? Error::None : Error::Load;
        emit self->changed();
    });
}

void FileExclusionController::addRule(const QString &pattern, const bool notificationEnabled) {
    if (!ready() || _saving || pattern.isEmpty()) {
        return;
    }
    // Adding an existing rule is a silent no-op: the dialog closes and nothing is sent to the server.
    if (duplicateRule(pattern)) {
        emit ruleAdded();
        return;
    }

    const auto key = normalizedKey(pattern);
    mutate(
            [pattern, key, notificationEnabled](std::vector<ExclusionTemplate> &templates) {
                const bool duplicate = std::ranges::any_of(templates, [&key](const ExclusionTemplate &exclusionTemplate) {
                    return normalizedKey(templatePattern(exclusionTemplate)) == key;
                });
                if (duplicate) {
                    return ExitInfo{ExitCode::Ok};
                }

                (void) templates.emplace_back(utf8String(pattern), notificationEnabled, false);
                return ExitInfo{ExitCode::Ok};
            },
            [this] { emit ruleAdded(); });
}

void FileExclusionController::setRuleNotification(const qint32 row, const bool notificationEnabled) {
    if (!ready() || _saving) {
        return;
    }

    const auto pattern = _userRules.patternAt(row);
    if (!pattern) {
        return;
    }
    const auto key = normalizedKey(*pattern);
    mutate([key, notificationEnabled](std::vector<ExclusionTemplate> &templates) {
        const auto it = std::ranges::find_if(templates, [&key](const ExclusionTemplate &exclusionTemplate) {
            return normalizedKey(templatePattern(exclusionTemplate)) == key;
        });
        if (it == templates.end()) {
            return ExitInfo{ExitCode::DataError, ExitCause::NotFound};
        }

        it->setWarning(notificationEnabled);
        return ExitInfo{ExitCode::Ok};
    });
}

void FileExclusionController::setSelected(const qint32 row, const bool selected) {
    if (ready() && !_saving) {
        _userRules.setSelected(row, selected);
    }
}

void FileExclusionController::selectAll() {
    if (ready() && !_saving) {
        _userRules.selectAll();
    }
}

void FileExclusionController::clearSelection() {
    if (ready() && !_saving) {
        _userRules.clearSelection();
    }
}

void FileExclusionController::removeSelected() {
    if (!ready() || _saving || selectedCount() == 0) {
        return;
    }

    const auto selectedKeys = _userRules.selectedPatternKeys();
    mutate([selectedKeys](std::vector<ExclusionTemplate> &templates) {
        std::erase_if(templates, [&selectedKeys](const ExclusionTemplate &exclusionTemplate) {
            return selectedKeys.contains(normalizedKey(templatePattern(exclusionTemplate)));
        });
        return ExitInfo{ExitCode::Ok};
    });
}

bool FileExclusionController::duplicateRule(const QString &pattern) const {
    const auto key = normalizedKey(pattern);
    const auto contains = [&key](const std::vector<ExclusionTemplate> &templates) {
        return std::ranges::any_of(templates, [&key](const ExclusionTemplate &exclusionTemplate) {
            return normalizedKey(templatePattern(exclusionTemplate)) == key;
        });
    };
    return contains(_service.defaultTemplates()) || contains(_service.userTemplates());
}

void FileExclusionController::mutate(const ExclusionTemplateService::UserMutation &mutation,
                                     const SuccessCallback &successCallback) {
    _saving = true;
    _error = Error::None;
    emit changed();

    _service.mutateUserTemplates(mutation, [self = QPointer(this), successCallback](const ExitInfo &result) {
        if (!self) {
            return;
        }

        self->_saving = false;
        self->_error = result ? Error::None : Error::Save;
        emit self->changed();
        if (result && successCallback) {
            successCallback();
        }
    });
}

void FileExclusionController::syncModels() {
    if (_service.defaultTemplatesLoaded()) {
        _defaultRules.setRules(_service.defaultTemplates(), false);
    } else {
        _defaultRules.setRules({}, false);
    }
    if (_service.userTemplatesLoaded()) {
        _userRules.setRules(_service.userTemplates(), true);
    } else {
        _userRules.setRules({}, false);
    }
    emit changed();
}

} // namespace KDC
