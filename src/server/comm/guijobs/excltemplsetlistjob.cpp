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
    constexpr auto logMessage = "Exception in ExclTemplSetListJob::readParamValue: error=";
    try {
        readParamValue(inParamsDefault, _default);
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


ExitInfo ExclTemplSetListJob::process() {
    std::vector<ExclusionTemplateInfo> expandedUserTemplateList;

    if (!_default) {
        expandedUserTemplateList = computeNormalizations(_exclusionTemplateList);
    } else {
        expandedUserTemplateList = _exclusionTemplateList;
    }

    if (const auto exitCode = ServerRequests::setExclusionTemplateList(_default, expandedUserTemplateList);
        exitCode != ExitCode::Ok) {
        LOG_WARN(_logger, "Error in Requests::setExclusionTemplateList: code=" << exitCode);
        AppServer::addError(Error(ERR_ID, exitCode));

        return exitCode;
    }

    return ExitCode::Ok;
}

std::vector<ExclusionTemplateInfo> ExclTemplSetListJob::computeNormalizations(
        const std::vector<ExclusionTemplateInfo> &templateList) {
    std::vector<ExclusionTemplateInfo> result;

    for (const auto &templateInfo: templateList) {
        const auto normalizations = computeNormalizations(Str2SyncName(templateInfo.templ().toStdString()));
        for (const auto &normalization: normalizations)
            result.push_back(ExclusionTemplateInfo{QString::fromStdString(SyncName2Str(normalization))});
    }

    return result;
}

//
//! Computes and returns all possible NFC and NFD normalizations of `templateString` segments
//! interpreted as a file system path.
/*!
  \param templateString is the pattern string the normalizations of which are queried.
  \return a set of std::string containing the NFC and NFD normalizations of exclusionTemplate, if those have been successful.
  The returned set contains additionally the string exclusionTemplate in any case.
*/
std::unordered_set<SyncName> ExclTemplSetListJob::computeNormalizations(const SyncName &templateString) {
    if (!canNormalize(templateString)) return {templateString};

    const auto normalizations = CommonUtility::computePathNormalizations(templateString);
    std::unordered_set<SyncName> result;
    for (const SyncName &normalization: normalizations) (void) result.emplace(normalization);
    return result;
}


bool ExclTemplSetListJob::canNormalize(const SyncName &template_) {
    SyncName nfcNormalizedName;
    const bool nfcSuccess = CommonUtility::normalizedSyncName(template_, nfcNormalizedName, UnicodeNormalization::NFC);
    if (!nfcSuccess) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in CommonUtility::normalizedSyncName. Failed to NFC-normalize the template '" << SyncName2Str(template_)
                                                                                                      << "'.");
    }

    SyncName nfdNormalizedName;
    const bool nfdSuccess = CommonUtility::normalizedSyncName(template_, nfdNormalizedName, UnicodeNormalization::NFD);
    if (!nfdSuccess) {
        LOG_WARN(Log::instance()->getLogger(),
                 "Error in CommonUtility::normalizedSyncName. Failed to NFD-normalize the template '" << SyncName2Str(template_)
                                                                                                      << "'.");
    }

    if (!nfcSuccess || !nfdSuccess) {
        LOG_WARN(Log::instance()->getLogger(),
                 "File exclusion based on user templates may fail to exclude file names depending on their normalizations.");
        return false;
    }
    return true;
}

} // namespace KDC
