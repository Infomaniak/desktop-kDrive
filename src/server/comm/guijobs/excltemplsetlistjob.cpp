/*ExclTemplSetUserListJob*/

#include "excltemplsetlistjob.h"
#include "useractionscopedlock.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "libcommonserver/log/log.h"

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
    std::list<std::shared_ptr<SyncPal>> syncPalList;

    {
        const std::scoped_lock lock(_commManager->appServer().syncPalMapMutex);
        for (const auto &[_, syncPal]: _commManager->appServer().syncPalMap) {
            if (!syncPal) continue;
            syncPalList.push_back(syncPal);
        }
    }

    std::list<UserActionScopedLock> locks;
    for (auto syncPal: syncPalList) {
        auto &lock = locks.emplace_back();
        if (!lock.tryLock(syncPal, std::chrono::milliseconds(userActionLockShortTimeoutMs))) {
            LOG_WARN(_logger, "Could not acquire user action lock for syncDbId="
                                      << syncPal->syncDbId()
                                      << ". Another user action is running. Aborting ExclTemplSetUserListJob.");
            return ExitCode::OperationCanceled;
        }
    }

    if (const auto exitInfo = ServerRequests::setUserExclusionTemplateList(_exclusionTemplateList); !exitInfo) {
        LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: " << exitInfo);
        addError(Error(ERR_ID, exitInfo));
        return exitInfo;
    }

    for (auto syncPal: syncPalList) {
        _commManager->appServer().unregisterSync(syncPal);

        if (const auto exitInfo = syncPal->propagateExcludeListChange(); !exitInfo) {
            LOG_WARN(_logger, "Error in SyncPal::PropagateExcludeListChange: code=" << exitInfo);
        }

        _commManager->appServer().registerSync(syncPal);
    }

    return ExitCode::Ok;
}

} // namespace KDC
