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

#include <functional>
#include <vector>

namespace KDC {

class ServiceEventBus;

/**
 * Server-confirmed exclusion-template snapshots and full-list user mutations.
 *
 * Both snapshots are cached for the application lifetime after their first successful load. Each successful user
 * mutation publishes only the list read back from the server.
 */
class ExclusionTemplateService final : public QObject {
        Q_OBJECT

    public:
        using CompletionCallback = CommService::VoidCallback;
        using UserMutation = std::function<ExitInfo(std::vector<ExclusionTemplate> &)>;

        ExclusionTemplateService(const CommService &commService, ServiceEventBus &eventBus, QObject *parent = nullptr);

        /** Returns true while both server-confirmed snapshots are available. */
        [[nodiscard]] bool ready() const { return _defaultTemplatesLoaded && _userTemplatesLoaded; }
        [[nodiscard]] bool defaultTemplatesLoaded() const { return _defaultTemplatesLoaded; }
        [[nodiscard]] bool userTemplatesLoaded() const { return _userTemplatesLoaded; }
        [[nodiscard]] const std::vector<ExclusionTemplate> &defaultTemplates() const { return _defaultTemplates; }
        [[nodiscard]] const std::vector<ExclusionTemplate> &userTemplates() const { return _userTemplates; }

        void ensureLoaded(const CompletionCallback &callback = {});

        /**
         * Applies a mutation by replacing the complete server-side user-template list.
         *
         * @warning DATA LOSS RISK: DO NOT CALL THIS METHOD OUTSIDE FileExclusionController. Concurrent calls can
         * silently overwrite confirmed user rules because the server replaces the complete list.
         */
        void mutateUserTemplates(const UserMutation &mutation, const CompletionCallback &callback = {});

    signals:
        void snapshotsChanged();

    private:
        void requestUserTemplates();
        void handleGetDefaultTemplatesResult(const ExitInfo &result, const std::vector<ExclusionTemplate> &defaultTemplates);
        void handleUserTemplatesLoadResult(const ExitInfo &result, const std::vector<ExclusionTemplate> &userTemplates);
        void finishLoading(const ExitInfo &result);
        void handleSetUserTemplatesResult(const ExitInfo &result, const CompletionCallback &callback);
        void handleUserTemplatesReadbackResult(const ExitInfo &result, const std::vector<ExclusionTemplate> &confirmedTemplates,
                                               const CompletionCallback &callback);

        const CommService &_commService;
        ServiceEventBus &_eventBus;
        std::vector<ExclusionTemplate> _defaultTemplates;
        std::vector<ExclusionTemplate> _userTemplates;
        std::vector<CompletionCallback> _loadCallbacks;
        bool _defaultTemplatesLoaded{false};
        bool _userTemplatesLoaded{false};
        bool _loading{false};
};

} // namespace KDC
