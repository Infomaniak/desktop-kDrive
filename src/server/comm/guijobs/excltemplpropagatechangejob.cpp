/*ExclTemplPropagateChangeJob*/

#include "excltemplpropagatechangejob.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "libcommon/log/log.h"

namespace KDC {

ExclTemplPropagateChangeJob::ExclTemplPropagateChangeJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                         const Poco::DynamicStruct &inParams,
                                                         std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLTEMPL_PROPAGATE_CHANGE;
}


ExitInfo ExclTemplPropagateChangeJob::process() {
    const std::scoped_lock lock(AppServer::syncPalMapMutex);
    for (const auto &[_, syncPal]: AppServer::syncPalMap) {
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
