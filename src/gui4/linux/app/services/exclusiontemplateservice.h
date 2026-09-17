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

#include "app/services/commservice.h"

#include <QObject>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace KDC {

class ServiceEventBus;

/** Fresh server-confirmed exclusion-template snapshots and serialized full-list user mutations. */
class ExclusionTemplateService final : public QObject {
        Q_OBJECT

    public:
        using CompletionCallback = CommService::VoidCallback;
        using UserMutation = std::function<ExitInfo(std::vector<ExclusionTemplate> &)>;

        ExclusionTemplateService(const CommService &commService, ServiceEventBus &eventBus, QObject *parent = nullptr);

        /** Returns true only while both snapshots belong to the latest successful refresh. */
        [[nodiscard]] bool ready() const { return _defaultTemplatesLoaded && _userTemplatesLoaded; }
        [[nodiscard]] bool defaultTemplatesLoaded() const { return _defaultTemplatesLoaded; }
        [[nodiscard]] bool userTemplatesLoaded() const { return _userTemplatesLoaded; }
        [[nodiscard]] const std::vector<ExclusionTemplate> &defaultTemplates() const { return _defaultTemplates; }
        [[nodiscard]] const std::vector<ExclusionTemplate> &userTemplates() const { return _userTemplates; }

        void refresh(const CompletionCallback &callback = {});
        void mutateUserTemplates(const UserMutation &mutation, const CompletionCallback &callback = {});

    signals:
        void snapshotsChanged();

    private:
        struct RefreshState;
        struct PendingMutation {
                UserMutation mutation;
                CompletionCallback callback;
        };

        void finishRefresh(const std::shared_ptr<RefreshState> &state);
        void startNextMutation();
        void finishCurrentMutation(const ExitInfo &result,
                                   const std::optional<std::vector<ExclusionTemplate>> &confirmedTemplates = std::nullopt);

        const CommService &_commService;
        ServiceEventBus &_eventBus;
        std::vector<ExclusionTemplate> _defaultTemplates;
        std::vector<ExclusionTemplate> _userTemplates;
        std::vector<CompletionCallback> _refreshCallbacks;
        std::deque<PendingMutation> _mutations;
        bool _defaultTemplatesLoaded{false};
        bool _userTemplatesLoaded{false};
        bool _refreshing{false};
        bool _mutating{false};
};

} // namespace KDC
