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
#include "libcommon/comm.h"

#include <Poco/JSON/Parser.h>
#include <Poco/Exception.h>
#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>

static const auto inRequestId = "id";
static const auto inRequestNum = "num";
static const auto inRequestParams = "params";

static const auto outRequestCode = "code";
static const auto outRequestCause = "cause";
static const auto outRequestParams = "params";

namespace KDC {

AbstractGuiJob::AbstractGuiJob(std::shared_ptr<CommManager> commManager, const std::string &inputParamsStr,
                               const std::shared_ptr<AbstractCommChannel> &channel) :
    _commManager(commManager),
    _inputParamsStr(inputParamsStr),
    _channel(channel) {}

void AbstractGuiJob::runJob() {
    _exitInfo = ExitCode::Ok;
    if (_type == GuiJobType::Query) {
        if (!deserializeInputParms()) {
            LOG_WARN(Log::instance()->getLogger(), "Error in AbstractGuiJob::deserializeInputParms for job=" << jobId());
        }

        if (_exitInfo && !process()) {
            LOG_WARN(Log::instance()->getLogger(), "Error in AbstractGuiJob::process for job=" << jobId());
        }

        if (!serializeOutputParms()) {
            LOG_WARN(Log::instance()->getLogger(), "Error in AbstractGuiJob::serializeOutputParms for job=" << jobId());
        }

        _channel->sendMessage(CommonUtility::str2CommString(_outputParamsStr));
    } else if (_type == GuiJobType::Signal) {
        _channel->sendMessage(_inputParamsStr);
        _exitInfo = ExitCode::Ok;
    } else {
        LOG_WARN(_logger, "The job type must be set");
        _exitInfo = ExitCode::LogicError;
    };
}

bool AbstractGuiJob::deserializeInputParms() {
    try {
        Poco::JSON::Parser parser;
        Poco::Dynamic::Var inputParamsVar = parser.parse(_inputParamsStr);

        Poco::DynamicStruct paramsStruct = *inputParamsVar.extract<Poco::JSON::Object::Ptr>();

        AbstractGuiJob::readParamValue(paramsStruct, inRequestId, _requestId);
        AbstractGuiJob::readParamValue(paramsStruct, inRequestNum, _requestNum);

        _inParams = paramsStruct[inRequestParams].extract<Poco::DynamicStruct>();
    } catch (std::exception &e) {
        LOG_WARN(_logger, "Input parameters deserialization error for job=" << jobId() << " error=" << e.what());
        _exitInfo = ExitCode::LogicError;
        return false;
    }

    return true;
}

bool AbstractGuiJob::serializeOutputParms() {
    Poco::DynamicStruct paramsStruct;
    AbstractGuiJob::writeParamValue(paramsStruct, outRequestCode, _exitInfo.code());
    AbstractGuiJob::writeParamValue(paramsStruct, outRequestCause, _exitInfo.cause());
    paramsStruct.insert(outRequestParams, _outParams);

    try {
        _outputParamsStr = Poco::Dynamic::structToString(paramsStruct);
        return true;
    } catch (Poco::Exception &e) {
        LOG_WARN(_logger, "Output parameters serialization error for job=" << jobId() << " error=" << e.what());
        _exitInfo = ExitCode::LogicError;
        return false;
    }
}

bool AbstractGuiJob::process() {
    return true;
}

void AbstractGuiJob::convertFromBase64Str(const std::string &base64Str, std::string &value) {
    std::istringstream istr(base64Str);
    Poco::Base64Decoder b64in(istr);
    b64in >> value;
}

void AbstractGuiJob::convertFromBase64Str(const std::string &base64Str, std::wstring &value) {
    std::string strValue;
    AbstractGuiJob::convertFromBase64Str(base64Str, strValue);
    value = CommonUtility::s2ws(strValue);
}

void AbstractGuiJob::convertFromBase64Str(const std::string &base64Str, CommBLOB &value) {
    std::istringstream istr(base64Str);
    Poco::Base64Decoder b64in(istr);
    std::copy(std::istream_iterator<unsigned char>(b64in), std::istream_iterator<unsigned char>(), std::back_inserter(value));
}

void AbstractGuiJob::convertToBase64Str(const std::string &str, std::string &base64Str) {
    std::ostringstream ostr;
    Poco::Base64Encoder b64out(ostr);
    b64out << str;
    b64out.close();
    base64Str = ostr.str();
}

void AbstractGuiJob::convertToBase64Str(const std::wstring &wstr, std::string &base64Str) {
    std::string str = CommonUtility::ws2s(wstr);
    AbstractGuiJob::convertToBase64Str(str, base64Str);
}

void AbstractGuiJob::convertToBase64Str(const CommBLOB &blob, std::string &base64Str) {
    std::ostringstream ostr;
    Poco::Base64Encoder b64out(ostr);
    std::copy(blob.begin(), blob.end(), std::ostream_iterator<unsigned char>(b64out));
    b64out.close();
    base64Str = ostr.str();
}

} // namespace KDC
