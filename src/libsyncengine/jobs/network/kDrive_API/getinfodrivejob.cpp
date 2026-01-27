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

#include "getinfodrivejob.h"

#include "utility/jsonparserutility.h"

#include <Poco/Net/HTTPRequest.h>

namespace KDC {

static const std::string isLockedKey = "is_locked";
static const std::string usedSizeKey = "used_size";
static const std::string packKey = "pack";
static const std::string packDisplayNameKey = "display_name";
static const std::string packIsFreeKey = "is_free";

GetInfoDriveJob::GetInfoDriveJob(int userDbId, int driveId) :
    AbstractTokenNetworkJob(ApiType::Drive, userDbId, 0, 0, driveId) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

GetInfoDriveJob::GetInfoDriveJob(int driveDbId) :
    AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
}

ExitInfo GetInfoDriveJob::handleError(const std::string &replyBody, const Poco::URI &uri) {
    if (httpResponse().getStatus() == Poco::Net::HTTPResponse::HTTP_FORBIDDEN ||
        httpResponse().getStatus() == Poco::Net::HTTPResponse::HTTP_NOT_FOUND) {
        // The drive is not accessible or doesn't exist
        return ExitCode::Ok;
    }
    return AbstractTokenNetworkJob::handleError(replyBody, uri);
}

ExitInfo GetInfoDriveJob::handleJsonResponse(const std::string &replyBody) {
    if (const auto exitInfo = AbstractTokenNetworkJob::handleJsonResponse(replyBody); !exitInfo) return exitInfo;

    Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
    if (!dataObj || dataObj->size() == 0) {
        LOG_WARN(Log::instance()->getLogger(), "Unable to read drive info");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, nameKey, _name)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, sizeKey, _size)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, accountAdminKey, _isAdmin)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (Poco::JSON::Object::Ptr prefObj = dataObj->getObject(preferencesKey)) { // Not mandatory
        (void) JsonParserUtility::extractValue(prefObj, colorKey, _colorHex, false);
    }

    const auto accountObj = dataObj->getObject(accountKey);
    if (!accountObj) {
        LOG_ERROR(Log::instance()->getLogger(), "Missing account info!");
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    if (!JsonParserUtility::extractValue(accountObj, idKey, _accountId)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }
    if (!JsonParserUtility::extractValue(accountObj, nameKey, _accountName)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    // Non DB attributes
    if (!JsonParserUtility::extractValue(dataObj, inMaintenanceKey, _isInMaintenance)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (_isInMaintenance) {
        (void) JsonParserUtility::extractValue(dataObj, maintenanceAtKey, _maintenanceFrom);
    }

    if (!JsonParserUtility::extractValue(dataObj, isLockedKey, _isLocked)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (!JsonParserUtility::extractValue(dataObj, usedSizeKey, _usedSize)) {
        return {ExitCode::BackError, ExitCause::MissingReplyData};
    }

    if (Poco::JSON::Object::Ptr packObj = dataObj->getObject(packKey); packObj) { // Not mandatory
        (void) JsonParserUtility::extractValue(packObj, idKey, _packInfo.id, false);
        (void) JsonParserUtility::extractValue(packObj, nameKey, _packInfo.name, false);
        (void) JsonParserUtility::extractValue(packObj, packDisplayNameKey, _packInfo.displayName, false);
        (void) JsonParserUtility::extractValue(packObj, packIsFreeKey, _packInfo.isFree, false);
    }

    return ExitCode::Ok;
}

void GetInfoDriveJob::setQueryParameters(Poco::URI &uri) {
    uri.addQueryParameter("with", "preferences,pack,account");
}

} // namespace KDC
