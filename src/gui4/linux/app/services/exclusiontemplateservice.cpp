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

#include "exclusiontemplateservice.h"

#include "app/services/serviceeventbus.h"

#include <QPointer>

#include <utility>

namespace KDC {

struct ExclusionTemplateService::RefreshState {
        std::optional<ExitInfo> defaultResult;
        std::optional<ExitInfo> userResult;
        std::vector<ExclusionTemplate> defaultTemplates;
        std::vector<ExclusionTemplate> userTemplates;
};

ExclusionTemplateService::ExclusionTemplateService(const CommService &commService, ServiceEventBus &eventBus,
                                                   QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _eventBus(eventBus) {}

void ExclusionTemplateService::refresh(const CompletionCallback &callback) {
    if (callback) {
        _refreshCallbacks.push_back(callback);
    }
    if (_refreshing) {
        return;
    }

    _refreshing = true;
    _defaultTemplatesLoaded = false;
    _userTemplatesLoaded = false;
    emit snapshotsChanged();

    // Both lists are requested in parallel; the refresh completes once both answers have arrived.
    const auto state = std::make_shared<RefreshState>();
    const auto receiveList = [self = QPointer(this), state](const bool defaultTemplates, const ExitInfo &result,
                                                            const std::vector<ExclusionTemplate> &templates) {
        if (!self) {
            return;
        }

        if (defaultTemplates) {
            state->defaultResult = result;
            state->defaultTemplates = templates;
        } else {
            state->userResult = result;
            state->userTemplates = templates;
        }

        if (state->defaultResult && state->userResult) {
            self->finishRefresh(state);
        }
    };

    constexpr bool defaultExclusionTemplates = true;
    constexpr bool userExclusionTemplates = false;

    _commService.requestExclTemplGetList(defaultExclusionTemplates,
                                         [receiveList](const ExitInfo &result, const std::vector<ExclusionTemplate> &templates) {
                                             receiveList(defaultExclusionTemplates, result, templates);
                                         });
    _commService.requestExclTemplGetList(userExclusionTemplates,
                                         [receiveList](const ExitInfo &result, const std::vector<ExclusionTemplate> &templates) {
                                             receiveList(userExclusionTemplates, result, templates);
                                         });
}

void ExclusionTemplateService::finishRefresh(const std::shared_ptr<RefreshState> &state) {
    const ExitInfo &defaultResult = *state->defaultResult;
    const ExitInfo &userResult = *state->userResult;

    if (defaultResult) {
        _defaultTemplates = state->defaultTemplates;
        _defaultTemplatesLoaded = true;
    } else {
        _eventBus.notifyGenericError(defaultResult, RequestNum::EXCLTEMPL_GETLIST);
    }

    if (userResult) {
        _userTemplates = state->userTemplates;
        _userTemplatesLoaded = true;
    } else {
        _eventBus.notifyGenericError(userResult, RequestNum::EXCLTEMPL_GETLIST);
    }

    // Detach the pending callbacks first: a snapshotsChanged handler may start a new refresh with its own callback.
    const auto callbacks = std::exchange(_refreshCallbacks, {});
    _refreshing = false;
    emit snapshotsChanged();

    // Report the default-list failure first, otherwise the user-list result.
    const ExitInfo result = defaultResult ? userResult : defaultResult;
    for (const auto &pendingCallback: callbacks) {
        pendingCallback(result);
    }
}

void ExclusionTemplateService::mutateUserTemplates(const UserMutation &mutation, const CompletionCallback &callback) {
    if (!mutation) {
        if (callback) {
            callback({ExitCode::LogicError, ExitCause::InvalidArgument});
        }
        return;
    }

    _mutations.push_back({.mutation = mutation, .callback = callback});
    startNextMutation();
}

void ExclusionTemplateService::startNextMutation() {
    if (_mutating || _mutations.empty()) {
        return;
    }

    if (!ready()) {
        finishCurrentMutation({ExitCode::InvalidOperation});
        return;
    }

    _mutating = true;
    auto templates = _userTemplates;
    if (const auto mutationResult = _mutations.front().mutation(templates); !mutationResult) {
        finishCurrentMutation(mutationResult);
        return;
    }

    if (templates == _userTemplates) {
        finishCurrentMutation({ExitCode::Ok});
        return;
    }

    // The server replaces the whole list; reading it back publishes the normalized form it actually stored.
    _commService.requestExclTemplSetList(templates, [self = QPointer(this)](const ExitInfo &setResult) {
        if (!self) {
            return;
        }

        if (!setResult) {
            self->_eventBus.notifyGenericError(setResult, RequestNum::EXCLTEMPL_SETUSERLIST);
            self->finishCurrentMutation(setResult);
            return;
        }

        self->_commService.requestExclTemplGetList(
                false, [self](const ExitInfo &readResult, const std::vector<ExclusionTemplate> &confirmedTemplates) {
                    if (!self) {
                        return;
                    }

                    if (!readResult) {
                        self->_userTemplatesLoaded = false;
                        self->_eventBus.notifyGenericError(readResult, RequestNum::EXCLTEMPL_GETLIST);
                        emit self->snapshotsChanged();
                        self->finishCurrentMutation(readResult);
                        return;
                    }

                    self->finishCurrentMutation(readResult, confirmedTemplates);
                });
    });
}

void ExclusionTemplateService::finishCurrentMutation(const ExitInfo &result,
                                                     const std::optional<std::vector<ExclusionTemplate>> &confirmedTemplates) {
    if (_mutations.empty()) {
        _mutating = false;
        return;
    }

    const auto callback = _mutations.front().callback;
    _mutations.pop_front();

    if (result && confirmedTemplates) {
        _userTemplates = *confirmedTemplates;
        _userTemplatesLoaded = true;
        emit snapshotsChanged();
    }

    if (callback) {
        callback(result);
    }

    _mutating = false;
    startNextMutation();
}

} // namespace KDC
