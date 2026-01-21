/*ExclTemplGetListJob*/

#include "excltemplgetlistjob.h"
#include "appserver.h"

#include "libcommon/comm.h"
#include "../../../libcommon/log/log.h"

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
    constexpr auto logMessage = "Exception in ExclTemplGetListJob::readParamValue: error=";
    try {
        readParamValue(inParamsDefault, _default);
    } catch (const Poco::Exception &pocoException) {
        LOG_WARN(_logger, logMessage << pocoException.message());

        return ExitCode::LogicError;
    } catch (const CommonUtility::InvalidEnumerationValue &cuException) {
        LOG_WARN(_logger, logMessage << cuException.what());

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
    _exclusionTemplateList = filterOutTemplatesWrtNfcNormalization(_exclusionTemplateList);

    return ExitCode::Ok;
}

std::vector<ExclusionTemplateInfo> ExclTemplGetListJob::filterOutTemplatesWrtNfcNormalization(
        const std::vector<ExclusionTemplateInfo> &templateList) {
    std::vector<ExclusionTemplateInfo> result;

    SyncNameSet uniqueTemplateNames; // Unique template names up to NFC-encoding.
    for (const auto &templateInfo: templateList) {
        SyncName normalizedName;
        SyncName insertedName = QStr2SyncName(templateInfo.templ());
        std::string insertedString;

        if (const bool nfcSuccess = CommonUtility::normalizedSyncName(insertedName, normalizedName, UnicodeNormalization::NFC);
            !nfcSuccess) {
            LOG_WARN(_logger, "Failed to NFC-normalize the template " << templateInfo.templ().toStdString());
            insertedString = templateInfo.templ().toStdString();
        } else {
            insertedName = normalizedName;
            insertedString = SyncName2Str(normalizedName);
        }

        const bool isNew = uniqueTemplateNames.emplace(insertedName).second;
        if (isNew) result.push_back(ExclusionTemplateInfo{QString::fromStdString(insertedString)});
    }

    return result;
}

} // namespace KDC
