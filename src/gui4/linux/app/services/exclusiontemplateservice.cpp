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

ExclusionTemplateService::ExclusionTemplateService(const CommService &commService, ServiceEventBus &eventBus,
                                                   QObject *const parent) :
    QObject(parent),
    _commService(commService),
    _eventBus(eventBus) {}

void ExclusionTemplateService::ensureLoaded(const CompletionCallback &callback) {
    if (ready()) {
        if (callback) {
            callback({ExitCode::Ok});
        }
        return;
    }

    if (callback) {
        _loadCallbacks.push_back(callback);
    }
    if (_loading) {
        return;
    }

    _loading = true;

    if (_defaultTemplatesLoaded) {
        requestUserTemplates();
        return;
    }

    constexpr bool defaultExclusionTemplates = true;
    _commService.requestExclTemplGetList(
            defaultExclusionTemplates,
            [self = QPointer(this)](const ExitInfo &result, const std::vector<ExclusionTemplate> &templates) {
                if (self) {
                    self->handleGetDefaultTemplatesResult(result, templates);
                }
            });
}

void ExclusionTemplateService::requestUserTemplates() {
    constexpr bool userExclusionTemplates = false; // true is for defaultExclusionTemplate
    _commService.requestExclTemplGetList(
            userExclusionTemplates,
            [self = QPointer(this)](const ExitInfo &result, const std::vector<ExclusionTemplate> &templates) {
                if (self) {
                    self->handleUserTemplatesLoadResult(result, templates);
                }
            });
}

void ExclusionTemplateService::handleGetDefaultTemplatesResult(const ExitInfo &result,
                                                               const std::vector<ExclusionTemplate> &defaultTemplates) {
    if (!result) {
        _eventBus.notifyGenericError(result, RequestNum::EXCLTEMPL_GETLIST);
        finishLoading(result);
        return;
    }

    _defaultTemplates = defaultTemplates;
    _defaultTemplatesLoaded = true;
    requestUserTemplates();
}

void ExclusionTemplateService::handleUserTemplatesLoadResult(const ExitInfo &result,
                                                             const std::vector<ExclusionTemplate> &userTemplates) {
    if (result) {
        _userTemplates = userTemplates;
        _userTemplatesLoaded = true;
    } else {
        _eventBus.notifyGenericError(result, RequestNum::EXCLTEMPL_GETLIST);
    }

    finishLoading(result);
}

void ExclusionTemplateService::finishLoading(const ExitInfo &result) {
    // Detach the pending callbacks first: a snapshotsChanged handler may start a new load with its own callback.
    const auto callbacks = std::exchange(_loadCallbacks, {});
    _loading = false;
    emit snapshotsChanged();

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
                    self->handleUserTemplatesReadbackResult(readResult, confirmedTemplates, callback);
                }
            });
}

void ExclusionTemplateService::handleUserTemplatesReadbackResult(const ExitInfo &result,
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
