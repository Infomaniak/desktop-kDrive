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

#include "abstractguijob.h"
#include "utility/jsonparserutility.h"

#include <Poco/JSON/Parser.h>
#include <Poco/Exception.h>

// Input parameters keys
static const auto inRequestId = "id";
static const auto inRequestNum = "num";
static const auto inRequestParams = "params";

// Output parameters keys
static const auto outRequestType = "type";
static const auto outRequestId = "id";
static const auto outRequestNum = "num";
static const auto outRequestCode = "code";
static const auto outRequestCause = "cause";
static const auto outRequestParams = "params";

namespace KDC {

int AbstractGuiJob::_lastSignalId = 0;

AbstractGuiJob::AbstractGuiJob(std::shared_ptr<CommManager> commManager, int requestId, const Poco::DynamicStruct &inParams,
                               const std::shared_ptr<AbstractCommChannel> channel) :
    _commManager(commManager),
    _requestId(requestId),
    _inParams(inParams),
    _channel(channel),
    _type(GuiJobType::Query) {}

AbstractGuiJob::AbstractGuiJob(std::shared_ptr<CommManager> commManager, const std::shared_ptr<AbstractCommChannel> channel) :
    _commManager(commManager),
    _channel(channel),
    _type(GuiJobType::Signal) {
    _signalId = _lastSignalId++;
}

void AbstractGuiJob::runJob() {
    if (_type == GuiJobType::Unknown) {
        LOG_WARN(_logger, "The job type must be set");
        _exitInfo = ExitCode::LogicError;
        return;
    }

    _exitInfo = ExitCode::Ok;
    if (_type == GuiJobType::Query) {
        if (!deserializeInputParms()) {
            LOG_WARN(_logger, "Error in deserializeInputParms for job=" << jobId());
        }

        if (_exitInfo && !process()) {
            LOG_WARN(_logger, "Error in process for job=" << jobId());
        }
    }

    if (!serializeOutputParms()) {
        LOG_WARN(_logger, "Error in serializeOutputParms for job=" << jobId());
    }

    if (_type == GuiJobType::Signal && !_exitInfo) return;

    if (!serializeGenericOutputParms()) {
        LOG_WARN(_logger, "Error in serializeGenericOutputParms for job=" << jobId());
    }

    if (!_channel->sendMessage(_outputParamsStr)) {
        LOG_WARN(_logger, "Error in AbstractCommChannel::sendMessage for job=" << jobId());
    }
}

bool AbstractGuiJob::deserializeGenericInputParms(const CommString &inputParamsStr, int &requestId, RequestNum &requestNum,
                                                  Poco::DynamicStruct &inParams) {
    try {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var inputParamsVar = parser.parse(CommonUtility::commString2Str(inputParamsStr));

        Poco::DynamicStruct paramsStruct = *inputParamsVar.extract<Poco::JSON::Object::Ptr>();

        CommonUtility::readValueFromStruct(paramsStruct, inRequestId, requestId);
        CommonUtility::readValueFromStruct(paramsStruct, inRequestNum, requestNum);

        assert(paramsStruct[inRequestParams].isStruct());
        inParams = paramsStruct[inRequestParams].extract<Poco::DynamicStruct>();
    } catch (std::exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in CommonUtility::readValueFromStruct: error=" << e.what());
        return false;
    }

    return true;
}

bool AbstractGuiJob::serializeGenericOutputParms() {
    Poco::DynamicStruct paramsStruct;
    if (_type == GuiJobType::Query) {
        CommonUtility::writeValueToStruct(paramsStruct, outRequestType, GuiJobType::Query);
        CommonUtility::writeValueToStruct(paramsStruct, outRequestId, _requestId);
        CommonUtility::writeValueToStruct(paramsStruct, outRequestNum, _requestNum);
        CommonUtility::writeValueToStruct(paramsStruct, outRequestCode, _exitInfo.code());
        CommonUtility::writeValueToStruct(paramsStruct, outRequestCause, _exitInfo.cause());
    } else {
        CommonUtility::writeValueToStruct(paramsStruct, outRequestType, GuiJobType::Signal);
        CommonUtility::writeValueToStruct(paramsStruct, outRequestId, _signalId);
        CommonUtility::writeValueToStruct(paramsStruct, outRequestNum, _signalNum);
    }

    paramsStruct.insert(outRequestParams, _outParams);

    try {
        _outputParamsStr = CommonUtility::str2CommString(Poco::Dynamic::structToString(paramsStruct));
    } catch (Poco::Exception &e) {
        LOG_WARN(Log::instance()->getLogger(), "Error in Poco::Dynamic::structToString: error=" << e.what());
        return false;
    }

    return true;
}

} // namespace KDC
