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

#include "utilitysetappstatejob.h"
#include "appserver.h"
#include "libcommon/comm.h"
#include "../../../libcommon/log/log.h"

// Input parameters keys
static const auto inParamsKey = "key";
static const auto inParamsValue = "value";

namespace KDC {

UtilitySetAppStateJob::UtilitySetAppStateJob(std::shared_ptr<CommManager> commManager, int requestId,
                                             const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::UTILITY_SET_APPSTATE;
}

ExitInfo UtilitySetAppStateJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in UtilitySetAppStateJob::readParamValue: error=";
    try {
        readParamValue(inParamsKey, _key);
        readParamValue(inParamsValue, _value);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo UtilitySetAppStateJob::serializeOutputParms() {
    return ExitCode::Ok;
}

ExitInfo UtilitySetAppStateJob::process() {
    if (bool found = true; !ParmsDb::instance()->updateAppState(_key, _value, found)) {
        LOG_WARN(_logger, "Error in ParmsDb::updateAppState");
        AppServer::addError(Error(ERR_ID, ExitCode::DbError, ExitCause::DbAccessError));
        return ExitCode::DbError;
    } else if (!found) {
        LOG_WARN(_logger, _key << " not found in appState table");
        return ExitCode::DataError;
    }

    return ExitCode::Ok;
}

} // namespace KDC
