/*ExclTemplGetListJob*/

#include "excltemplgetlistjob.h"
#include "appserver.h"

#include "libsyncengine/requests/exclusiontemplatecache.h"

#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

// Input parameters keys
static const auto inParamsDefault = "default";

// Output parameters keys
static const auto outParamsExclusionTemplateList = "exclusionTemplateList";


namespace KDC {

ExclTemplGetListJob::ExclTemplGetListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                         const Poco::DynamicStruct &inParams, std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLTEMPL_GETLIST;
}

ExitInfo ExclTemplGetListJob::deserializeInputParms() {
    try {
        readParamValue(inParamsDefault, _default);
    } catch (const std::exception &e) {
        LOG_WARN(_logger, "Exception in ExclTemplGetListJob::readParamValue: error=" << e.what());
        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ExclTemplGetListJob::serializeOutputParms() {
    writeParamValues(outParamsExclusionTemplateList, _exclusionTemplateList, info2DynamicVar<ExclusionTemplateInfo>);

    return ExitCode::Ok;
}

ExitInfo ExclTemplGetListJob::process() {
    if (const auto exitCode = ServerRequests::getExclusionTemplateList(_default, _exclusionTemplateList);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::getExclusionTemplateList: code=" << exitCode);
        AppServer::addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    return ExitCode::Ok;
}

} // namespace KDC
