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

#pragma once

#include "app/services/exclusiontemplateservice.h"
#include "app/settings/exclusionrulemodel.h"

#include <QObject>

#include <cstdint>
#include <functional>

namespace KDC {

/** Presentation, validation, selection, and immediate-save actions for file-exclusion rules. */
class FileExclusionController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(ExclusionRuleModel *defaultRules READ defaultRules CONSTANT)
        Q_PROPERTY(ExclusionRuleModel *userRules READ userRules CONSTANT)
        Q_PROPERTY(bool ready READ ready NOTIFY changed)
        Q_PROPERTY(bool loading READ loading NOTIFY changed)
        Q_PROPERTY(bool saving READ saving NOTIFY changed)
        Q_PROPERTY(qint32 selectedCount READ selectedCount NOTIFY changed)
        Q_PROPERTY(qint32 userRuleCount READ userRuleCount NOTIFY changed)
        Q_PROPERTY(Qt::CheckState selectionCheckState READ selectionCheckState NOTIFY changed)
        Q_PROPERTY(QString errorTextId READ errorTextId NOTIFY changed)

    public:
        explicit FileExclusionController(ExclusionTemplateService &service, QObject *parent = nullptr);

        [[nodiscard]] ExclusionRuleModel *defaultRules() { return &_defaultRules; }
        [[nodiscard]] ExclusionRuleModel *userRules() { return &_userRules; }
        [[nodiscard]] bool ready() const { return _service.ready(); }
        [[nodiscard]] bool loading() const { return _loading; }
        [[nodiscard]] bool saving() const { return _saving; }
        [[nodiscard]] qint32 selectedCount() const { return _userRules.selectedCount(); }
        [[nodiscard]] qint32 userRuleCount() const { return _userRules.count(); }
        [[nodiscard]] Qt::CheckState selectionCheckState() const;
        [[nodiscard]] QString errorTextId() const;

        Q_INVOKABLE void ensureLoaded();
        Q_INVOKABLE void addRule(const QString &pattern, bool notificationEnabled);
        Q_INVOKABLE void setRuleNotification(qint32 row, bool notificationEnabled);
        Q_INVOKABLE void setSelected(qint32 row, bool selected);
        Q_INVOKABLE void selectAll();
        Q_INVOKABLE void clearSelection();
        Q_INVOKABLE void removeSelected();

    signals:
        void changed();
        void ruleAdded();

    private:
        enum class Error : uint8_t {
            None,
            Load,
            Save,
        };

        using SuccessCallback = std::function<void()>;

        [[nodiscard]] bool duplicateRule(const QString &pattern) const;
        void mutate(const ExclusionTemplateService::UserMutation &mutation, const SuccessCallback &successCallback = {});
        void syncModels();

        ExclusionTemplateService &_service;
        ExclusionRuleModel _defaultRules;
        ExclusionRuleModel _userRules;
        bool _loading{false};
        bool _saving{false};
        Error _error{Error::None};
};

} // namespace KDC
