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

#include <optional>
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
    // @warning DATA LOSS RISK: DO NOT CALL THIS METHOD OUTSIDE FileExclusionController. See exclusiontemplateservice.h
    if (!mutation) {
        if (callback) {
            callback({ExitCode::LogicError, ExitCause::InvalidArgument});
        }
        return;
    }

    if (!ready()) {
        if (callback) {
            callback({ExitCode::InvalidOperation});
        }
        return;
    }

    auto templates = _userTemplates;
    if (const auto mutationResult = mutation(templates); !mutationResult) {
        if (callback) {
            callback(mutationResult);
        }
        return;
    }

    if (templates == _userTemplates) {
        if (callback) {
            callback({ExitCode::Ok});
        }
        return;
    }

    // The server replaces the whole list; reading it back publishes the normalized form it actually stored.
    _commService.requestExclTemplSetList(templates, [self = QPointer(this), callback](const ExitInfo &setResult) {
        if (self) {
            self->handleSetUserTemplatesResult(setResult, callback);
        }
    });
}

void ExclusionTemplateService::handleSetUserTemplatesResult(const ExitInfo &result, const CompletionCallback &callback) {
    if (!result) {
        _eventBus.notifyGenericError(result, RequestNum::EXCLTEMPL_SETUSERLIST);
        if (callback) {
            callback(result);
        }
        return;
    }

    _commService.requestExclTemplGetList(
            false, [self = QPointer(this), callback](const ExitInfo &readResult,
                                                     const std::vector<ExclusionTemplate> &confirmedTemplates) {
                if (self) {
                    self->handleGetUserTemplatesResult(readResult, confirmedTemplates, callback);
                }
            });
}

void ExclusionTemplateService::handleGetUserTemplatesResult(const ExitInfo &result,
                                                            const std::vector<ExclusionTemplate> &confirmedTemplates,
                                                            const CompletionCallback &callback) {
    if (!result) {
        _userTemplatesLoaded = false;
        _eventBus.notifyGenericError(result, RequestNum::EXCLTEMPL_GETLIST);
        emit snapshotsChanged();
        if (callback) {
            callback(result);
        }
        return;
    }

    _userTemplates = confirmedTemplates;
    _userTemplatesLoaded = true;
    emit snapshotsChanged();

    if (callback) {
        callback(result);
    }
}

} // namespace KDC
