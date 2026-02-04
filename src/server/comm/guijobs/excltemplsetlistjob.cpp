/*ExclTemplSetUserListJob*/

#include "excltemplsetlistjob.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "libcommon/log/log.h"

// Input parameters keys
static const auto inParamsExclusionTemplateList = "exclusionTemplateList";

namespace KDC {

ExclTemplSetUserListJob::ExclTemplSetUserListJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                 const Poco::DynamicStruct &inParams,
                                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLTEMPL_SETUSERLIST;
}

ExitInfo ExclTemplSetUserListJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in ExclTemplSetUserListJob::readParamValue: error=";
    try {
        readParamValues(inParamsExclusionTemplateList, _exclusionTemplateList, dynamicVar2Struct<ExclusionTemplateInfo>);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}


ExitInfo ExclTemplSetUserListJob::process() {
    ExclusionTemplateInfo::updateExclusionTemplateInfoList(_exclusionTemplateList);

    if (const auto exitCode = ServerRequests::setUserExclusionTemplateList(_exclusionTemplateList);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: code=" << exitCode);
        addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
    for (const auto [_, syncPal]: _commManager->appServer().syncPalMap) {
        if (!syncPal) continue;

        _commManager->appServer().unregisterSync(syncPal);

        if (const auto exitCode = syncPal->excludeListUpdated(); exitCode != ExitCode::Ok) {
            LOG_WARN(_logger, "Error in SyncPal::excludeListUpdated: code=" << exitCode);
        }

        _commManager->appServer().registerSync(syncPal);
    }

    return ExitCode::Ok;
}

} // namespace KDC
