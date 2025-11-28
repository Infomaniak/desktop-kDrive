/*ExclTemplSetListJob*/

#include "excltemplsetlistjob.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDefault = "default";
static const auto inParamsExclusionTemplateList = "exclusionTemplateList";

namespace KDC {

ExclTemplSetListJob::ExclTemplSetListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLTEMPL_SETLIST;
}

ExitInfo ExclTemplSetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDefault, _default);
        readParamValues(inParamsExclusionTemplateList, _exclusionTemplateList, dynamicVar2Struct<ExclusionTemplateInfo>);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ExclTemplSetListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}


ExitInfo ExclTemplSetListJob::process() {
    if (const auto exitCode = ServerRequests::setExclusionTemplateList(_default, _exclusionTemplateList);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: code=" << exitCode);
        AppServer::addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
