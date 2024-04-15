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

#include "csvfullfilelistwithcursorjob.h"

#include "libcommonserver/utility/utility.h"
#include "update_detection/file_system_observer/snapshot/snapshotitem.h"
#include "requests/parameterscache.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"

#include <Poco/JSON/Parser.h>

#define API_TIMEOUT 900

namespace KDC {

CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob(int driveDbId, const NodeId &dirId,
                                                           std::unordered_set<NodeId> blacklist /*= {}*/, bool zip /*= true*/)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0), _dirId(dirId), _blacklist(blacklist), _zip(zip) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = API_TIMEOUT + 15;

    if (_zip) {
        addRawHeader("Accept-Encoding", "gzip");
    }
}

bool CsvFullFileListWithCursorJob::getItem(SnapshotItem &item, bool &error, bool &ignore) {
    error = false;
    ignore = false;

    std::string line;
    std::getline(_ss, line);
    if (line.empty()) {
        return false;
    }

    if (_ignoreFirstLine) {
        std::getline(_ss, line);
        if (line.empty()) {
            return false;
        }
        _ignoreFirstLine = false;
    }

    CsvIndex index = CsvIndexId;
    bool readingDoubleQuotedValue = false;
    bool prevCharDoubleQuotes = false;
    bool readNextLine = true;
    std::string tmp;
    while (readNextLine) {
        readNextLine = false;

        for (char &c : line) {
            if (c == ',' && (!readingDoubleQuotedValue || prevCharDoubleQuotes)) {
                readingDoubleQuotedValue = false;
                prevCharDoubleQuotes = false;
                if (!updateSnapshotItem(tmp, index, item)) {
                    LOGW_WARN(_logger, L"Error in updateSnapshotItem - line=" << Utility::s2ws(line).c_str());
                    error = true;
                    return true;
                }
                tmp.clear();
                index = static_cast<CsvIndex>(static_cast<int>(index) + 1);
            } else if (c == '"') {
                if (!readingDoubleQuotedValue) {
                    readingDoubleQuotedValue = true;
                } else {
                    if (prevCharDoubleQuotes) {
                        // Replace 2 successive double quotes by one
                        prevCharDoubleQuotes = false;
                        tmp.push_back('"');
                    } else {
                        prevCharDoubleQuotes = true;
                    }
                }
            } else {
                tmp.push_back(c);
            }
        }

        // File name could have line return in it. If so, read next line and continue parsing
        if (readingDoubleQuotedValue) {
            tmp.push_back('\n');
            readNextLine = true;
            std::getline(_ss, line);
            if (line.empty()) {
                LOGW_WARN(_logger, L"Invalid line");
                ignore = true;
                return true;
            }
        }
    }

    if (index < CsvIndexEnd - 1) {
        LOGW_WARN(_logger, L"Invalid item");
        ignore = true;
        return true;
    }

    // Update last value
    if (!updateSnapshotItem(tmp, index, item)) {
        LOGW_WARN(_logger, L"Error in updateSnapshotItem - line=" << Utility::s2ws(line).c_str());
        error = true;
        return true;
    }

    return true;
}

std::string CsvFullFileListWithCursorJob::getCursor() {
    return _resHttp.get("X-kDrive-Cursor");
}

std::string CsvFullFileListWithCursorJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/listing/full";
    return str;
}

void CsvFullFileListWithCursorJob::setQueryParameters(Poco::URI &uri, bool &canceled) {
    uri.addQueryParameter("directory_id", _dirId);
    uri.addQueryParameter("recursive", "true");
    uri.addQueryParameter("format", "csv");
    if (!_blacklist.empty()) {
        std::string str = Utility::list2str(_blacklist);
        if (!str.empty()) {
            uri.addQueryParameter("without_ids", str);
        }
    }
    uri.addQueryParameter("with", "files.is_link");

    canceled = false;
}

bool CsvFullFileListWithCursorJob::handleResponse(std::istream &is) {
    if (_zip) {
        unzip(is, _ss);
    } else {
        _ss << is.rdbuf();
    }

    // Check that the stringstream ends by a LF (0x0A)
    _ss.seekg(0, std::ios_base::end);
    int length = _ss.tellg();
    if (length == 0) {
        // Folder is empty
        LOG_DEBUG(_logger, "Reply " << jobId() << " received - length=" << length);
        return true;
    }

    _ss.seekg(length - 1, std::ios_base::beg);
    char lastChar = 0x00;
    _ss.read(&lastChar, 1);
    _ss.seekg(0, std::ios_base::beg);
    if (lastChar != 0x0A) {
        LOGW_WARN(_logger, L"Reply " << jobId() << L" received with bad content - length=" << length
                                     << " value=" << Utility::s2ws(_ss.str()).c_str());
        return false;
    }

    if (isExtendedLog()) {
        LOGW_DEBUG(_logger,
                   L"Reply " << jobId() << L" received - length=" << length << " value=" << Utility::s2ws(_ss.str()).c_str());
    }

    return true;
}

bool CsvFullFileListWithCursorJob::updateSnapshotItem(const std::string &str, const CsvIndex index, SnapshotItem &item) {
    switch (index) {
        case CsvIndexId: {
            item.setId(str);
            break;
        }
        case CsvIndexParentId: {
            item.setParentId(str);
            break;
        }
        case CsvIndexName: {
            SyncName name = Str2SyncName(str);
#ifdef _WIN32
            SyncName newName;
            if (PlatformInconsistencyCheckerUtility::instance()->fixNameWithBackslash(name, newName)) {
                name = newName;
            }
#endif
            item.setName(name);
            break;
        }
        case CsvIndexType: {
            if (str == "dir") {
                item.setType(NodeTypeDirectory);
            } else {
                item.setType(NodeTypeFile);
            }
            break;
        }
        case CsvIndexSize: {
            try {
                item.setSize(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setSize - str=" << Utility::s2ws(str).c_str()
                                                                            << L" exc=invalid_argument err="
                                                                            << Utility::s2ws(ex.what()).c_str());
                return false;
            } catch (std::out_of_range const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setSize - str=" << Utility::s2ws(str).c_str()
                                                                            << L" exc=out_of_range err="
                                                                            << Utility::s2ws(ex.what()).c_str());
                return false;
            }
            break;
        }
        case CsvIndexCreatedAt: {
            try {
                item.setCreatedAt(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setCreatedAt - str=" << Utility::s2ws(str).c_str()
                                                                                 << L" exc=invalid_argument err="
                                                                                 << Utility::s2ws(ex.what()).c_str());
                return false;
            } catch (std::out_of_range const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setCreatedAt - str=" << Utility::s2ws(str).c_str()
                                                                                 << L" exc=out_of_range err="
                                                                                 << Utility::s2ws(ex.what()).c_str());
                return false;
            }
            break;
        }
        case CsvIndexModtime: {
            try {
                item.setLastModified(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setLastModified - str=" << Utility::s2ws(str).c_str()
                                                                                    << L" exc=invalid_argument err="
                                                                                    << Utility::s2ws(ex.what()).c_str());
                return false;
            } catch (std::out_of_range const &ex) {
                LOGW_WARN(_logger, L"Error in SnapshotItem::setLastModified - str=" << Utility::s2ws(str).c_str()
                                                                                    << L" exc=out_of_range err="
                                                                                    << Utility::s2ws(ex.what()).c_str());
                return false;
            }
            break;
        }
        case CsvIndexCanWrite: {
            item.setCanWrite(str == "1");
            break;
        }
        case CsvIndexIsLink: {
            item.setIsLink(str == "1");
            break;
        }
        default: {
            // Ignore additionnal columns
            break;
        }
    }
    return true;
}

}  // namespace KDC
