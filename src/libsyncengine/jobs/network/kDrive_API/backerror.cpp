/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
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

#include "backerror.h"

#include "jobs/network/networkjobsparams.h"
#include "libcommonserver/utility/jsonparserutility.h"

namespace KDC {

BackError::BackError(const Poco::JSON::Object::Ptr jsonObjPtr) {
    extractErrorFromJsonObject(jsonObjPtr);
}

BackError::BackError(const std::string &jsonStr) {
    Poco::JSON::Object::Ptr jsonObj;
    try {
        jsonObj = Poco::JSON::Parser{}.parse(jsonStr).extract<Poco::JSON::Object::Ptr>();
    } catch (const Poco::Exception &exc) {
        LOG_WARN(Log::instance()->getLogger(), "Fail to extract error from JSON: " << jsonStr << ", error: " << exc.what());
        return;
    }
    extractErrorFromJsonObject(jsonObj);
}

BackError::BackError(const std::string &code, const std::string &description, const std::string &contextReason /*= {}*/,
                     const std::string &contextModel /*= {}*/) :
    _code(code),
    _description(description),
    _contextReason(contextReason),
    _contextModel(contextModel) {}

void BackError::extractErrorFromJsonObject(const Poco::JSON::Object::Ptr jsonObjPtr) {
    if (!jsonObjPtr) return;
    if (jsonObjPtr->has(errorKey))
        extractFromFullReply(jsonObjPtr);
    else
        extractFromErrorObject(jsonObjPtr);
}

void BackError::extractFromFullReply(const Poco::JSON::Object::Ptr jsonObjPtr) {
    if (!jsonObjPtr) return;

    const auto errorObjPtr = jsonObjPtr->getObject(errorKey);
    return extractFromErrorObject(errorObjPtr);
}

void BackError::extractFromErrorObject(const Poco::JSON::Object::Ptr jsonObjPtr) {
    if (!jsonObjPtr) return;

    (void) JsonParserUtility::extractValue(jsonObjPtr, codeKey, _code, false);
    (void) JsonParserUtility::extractValue(jsonObjPtr, descriptionKey, _description, false);
    const auto contextObjPtr = jsonObjPtr->getObject(contextKey);
    (void) JsonParserUtility::extractValue(contextObjPtr, reasonKey, _contextReason, false);
    (void) JsonParserUtility::extractValue(contextObjPtr, modelKey, _contextModel, false);
}

} // namespace KDC
