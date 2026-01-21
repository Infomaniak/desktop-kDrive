/*ExclTemplGetExcludedJob*/

#include "excltemplgetexcludedjob.h"
#include "appserver.h"

#include "libsyncengine/requests/exclusiontemplatecache.h"

#include "libcommon/comm.h"
#include "../../../libcommon/log/log.h"

// Input parameters keys
static const auto inParamsName = "name";

// Output parameters keys
static const auto outParamsIsExcluded = "isExcluded";


namespace KDC {

ExclTemplGetExcludedJob::ExclTemplGetExcludedJob(std::shared_ptr<CommManager> commManager, int requestId,
                                                 const Poco::DynamicStruct &inParams,
                                                 std::shared_ptr<AbstractCommChannel> channel) :
    AbstractGuiJob(commManager, requestId, inParams, channel) {
    _requestNum = RequestNum::EXCLTEMPL_GETEXCLUDED;
}

ExitInfo ExclTemplGetExcludedJob::deserializeInputParms() {
    constexpr auto logMessage = "Exception in ExclTemplGetExcludedJob::readParamValue: error=";
    try {
        readParamValue(inParamsName, _name);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

        return ExitCode::LogicError;
    }

    return ExitCode::Ok;
}

ExitInfo ExclTemplGetExcludedJob::serializeOutputParms() {
    writeParamValue(outParamsIsExcluded, _isExcluded);

    return ExitCode::Ok;
}

ExitInfo ExclTemplGetExcludedJob::process() {
    bool isWarning = false;
    _isExcluded = ExclusionTemplateCache::instance()->isExcluded(_name, isWarning);

    return ExitCode::Ok;
}

} // namespace KDC
