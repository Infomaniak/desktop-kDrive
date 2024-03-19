/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
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

#include "createdirjob.h"
#include "libcommonserver/utility/utility.h"
#include "utility/jsonparserutility.h"

namespace KDC {

CreateDirJob::CreateDirJob(int driveDbId, const SyncPath &filepath, const NodeId &parentId, const SyncName &name,
                           const std::string &color /*= ""*/)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0),
      _filePath(filepath),
      _parentDirId(parentId),
      _name(name),
      _color(color) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

CreateDirJob::~CreateDirJob() {
    if (_vfsSetPinState && _vfsForceStatus) {
        if (!_vfsSetPinState(_filePath, PinStateAlwaysLocal)) {
            LOGW_WARN(_logger, L"Error in CreateDirJob::vfsSetPinState for path=" << Path2WStr(_filePath).c_str());
        }
        if (!_vfsForceStatus(_filePath, false, 0, true)) {
            LOGW_WARN(_logger, L"Error in CreateDirJob::vfsForceStatus for path=" << Path2WStr(_filePath).c_str());
        }
    }
}

std::string CreateDirJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _parentDirId;
    str += "/directory";
    return str;
}

void CreateDirJob::setData(bool &canceled) {
    Poco::JSON::Object json;
    json.set("name", _name);
    if (!_color.empty()) {
        json.set("color", _color);
    }

    std::stringstream ss;
    json.stringify(ss);
    _data = ss.str();
    canceled = false;
}

bool CreateDirJob::handleResponse(std::istream &is) {
    if (!AbstractTokenNetworkJob::handleResponse(is)) {
        return false;
    }

    if (jsonRes()) {
        Poco::JSON::Object::Ptr dataObj = jsonRes()->getObject(dataKey);
        if (dataObj) {
            if (!JsonParserUtility::extractValue(dataObj, idKey, _nodeId)) {
                return false;
            }
            if (!JsonParserUtility::extractValue(dataObj, lastModifiedAtKey, _modtime)) {
                return false;
            }
        }

        if (_vfsForceStatus) {
            if (!_vfsForceStatus(_filePath, false, 100, true)) {
                LOGW_WARN(_logger, L"Error in CreateDirJob::_vfsForceStatus for path=" << Path2WStr(_filePath).c_str());
            }
        }
    }

    return true;
}

}  // namespace KDC
