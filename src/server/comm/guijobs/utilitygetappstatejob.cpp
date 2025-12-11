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

#include "utilitygetappstatejob.h"
#include "appserver.h"
#include "requests/serverrequests.h"
#include "libcommon/utility/utility.h"
#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsKey = "key";

// Output parameters keys
static const auto outParamsValue = "value";

namespace KDC {

UtilityGetAppStateJob::UtilityGetAppStateJob(std::shared_ptr<CommManager> commManager, int requestId,
                                             const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_GET_APPSTATE;
}

ExitInfo UtilityGetAppStateJob::deserializeInputParms() {
    try {
        readParamValue(inParamsKey, _key);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in UtilityGetAppStateJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilityGetAppStateJob::serializeOutputParms() {
    std::visit(
            [this](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                writeParamValue(outParamsValue, std::get<T>(_value));
            },
            _value);

    return ExitCode::Ok;
}

ExitInfo UtilityGetAppStateJob::process() {
    if (bool found = false; !ParmsDb::instance()->selectAppState(_key, _value, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::selectAppState");
        AppServer::addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        return ExitCode::DbError;
    } else if (!found) {
        LOG_WARN(_logger, _key << " not found in appState table");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
