
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

#ifdef _WIN32
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#endif

#include <Poco/JSON/Parser.h>

#define API_TIMEOUT 900

namespace KDC {

SnapshotItemHandler::SnapshotItemHandler(log4cplus::Logger logger) : _logger(logger) {};

void SnapshotItemHandler::logError(const std::wstring &methodName, const std::wstring &stdErrorType, const std::string &str,
                                   const std::exception &exc) {
    const std::wstring header = L"Error in SnapshotItem::" + methodName;
    const std::wstring strStr = L"str='" + Utility::s2ws(str) + L"', ";
    const std::wstring excStr = L"exc='" + stdErrorType + L"', " + L"err=" + Utility::s2ws(exc.what()) + L"'.";

    const std::wstring msg = header + strStr + excStr;

    LOGW_WARN(_logger, msg.c_str());
}

bool SnapshotItemHandler::updateSnapshotItem(const std::string &str, CsvIndex index, SnapshotItem &item) {
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
                item.setType(NodeType::Directory);
            } else {
                item.setType(NodeType::File);
            }
            break;
        }
        case CsvIndexSize: {
            try {
                item.setSize(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &exc) {
                logError(L"setSize", L"invalid_argument", str, exc);
                return false;
            } catch (std::out_of_range const &exc) {
                logError(L"setSize", L"out_of_range", str, exc);
                return false;
            }

            if (item.size() < 0) {
                LOGW_WARN(_logger, L"Error in setSize, got a negative value - str='" << Utility::s2ws(str).c_str() << L"'");
                return false;
            }

            break;
        }
        case CsvIndexCreatedAt: {
            try {
                item.setCreatedAt(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &exc) {
                logError(L"setCreatedAt", L"invalid_argument", str, exc);
                return false;
            } catch (std::out_of_range const &exc) {
                logError(L"setCreatedAt", L"out_of_range", str, exc);
                return false;
            }

            if (item.createdAt() < 0) {
                LOGW_WARN(_logger, L"Error in setCreatedAt, got a negative value - str='" << Utility::s2ws(str).c_str() << L"'");
                return false;
            }

            break;
        }
        case CsvIndexModtime: {
            try {
                item.setLastModified(str.empty() ? 0 : std::stoll(str));
            } catch (std::invalid_argument const &exc) {
                logError(L"setLastModified", L"invalid_argument", str, exc);
                return false;
            } catch (std::out_of_range const &exc) {
                logError(L"setLastModified", L"out_of_range", str, exc);
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

void SnapshotItemHandler::readSnapshotItemFields(SnapshotItem &item, const std::string &line, bool &error, ParsingState &state) {
    for (char c : line) {
        if (state.readingDoubleQuotedValue && state.prevCharDoubleQuotes) {
            if (c != ',' && c != '"') {
                // After a closing double quote, we must have a comma or another double quote. Otherwise, ignore the line.
                state.index = CsvIndexId;                // Make sure that `state` is not equal to `CsvIndexEnd`.
                state.readingDoubleQuotedValue = false;  // Exit the `readingDoubleQuotedValue` mode.
                LOG_WARN(_logger, "Item '" << line.c_str()
                                           << "' ignored because a closing double quote character must be followed by a comma or "
                                              "another double quote");

                return;
            }
        }

        if (c == ',' && (!state.readingDoubleQuotedValue || state.prevCharDoubleQuotes)) {
            state.readingDoubleQuotedValue = false;
            state.prevCharDoubleQuotes = false;
            if (!updateSnapshotItem(state.tmp, state.index, item)) {
                LOGW_WARN(_logger, L"Error in readSnapshotItemFields - line='" << Utility::s2ws(line).c_str() << "L'.");
                error = true;
                return;
            }
            state.tmp.clear();
            incrementCsvIndex(state.index);
        } else if (c == '"') {
            if (state.index != CsvIndexName) {
                // Double quotes are only allowed within file and directory names.
                LOG_WARN(_logger,
                         "Item '" << line.c_str() << "' ignored because the '\"' character is only allowed in the name field");
                return;
            }

            ++state.doubleQuoteCount;
            if (!state.readingDoubleQuotedValue) {
                state.readingDoubleQuotedValue = true;
            } else {
                if (state.prevCharDoubleQuotes) {
                    // Replace 2 successive double quotes by one (https://www.ietf.org/rfc/rfc4180.txt)
                    state.prevCharDoubleQuotes = false;
                    state.tmp.push_back('"');
                } else {
                    state.prevCharDoubleQuotes = true;
                }
            }
        } else {
            state.tmp.push_back(c);
        }
    }
}

bool SnapshotItemHandler::getItem(SnapshotItem &item, std::stringstream &ss, bool &error, bool &ignore) {
    error = false;
    ignore = false;

    std::string line;
    std::getline(ss, line);
    if (line.empty()) {
        return false;
    }

    if (_ignoreFirstLine) {
        // The first line of the CSV full listing consists of the column names:
        // "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link"
        _ignoreFirstLine = false;
        line.clear();
        std::getline(ss, line);
        if (line.empty()) {
            return false;
        }
    }

    ParsingState state;

    while (state.readNextLine) {
        state.readNextLine = false;

        // Ignore the lines containing escaped double quotes
        if (line.find(R"(\")") != std::string::npos) {
            LOGW_WARN(_logger, L"Line containing an escaped double quotes, ignored it - line=" << Utility::s2ws(line).c_str());
            ignore = true;
            return true;
        }

        readSnapshotItemFields(item, line, error, state);
        if (error) return true;

        // A file name surrounded by double quotes can have a line return in it. If so, read next line and continue parsing
        if (state.readingDoubleQuotedValue) {
            if (ss.eof()) {
                LOG_WARN(_logger, "EOF file reached prematurely");
                error = true;
                return true;
            }

            state.tmp.push_back('\n');
            state.readNextLine = true;
            std::getline(ss, line);
        }
    }

    if (state.index < CsvIndexEnd - 1) {
        LOG_WARN(_logger, "Invalid item");
        ignore = true;
        return true;
    }

    // Update last value
    if (!updateSnapshotItem(state.tmp, state.index, item)) {
        LOGW_WARN(_logger, L"Error in updateSnapshotItem - line=" << Utility::s2ws(line).c_str());
        error = true;
        return true;
    }

    return true;
}

CsvFullFileListWithCursorJob::CsvFullFileListWithCursorJob(int driveDbId, const NodeId &dirId,
                                                           std::unordered_set<NodeId> blacklist /*= {}*/, bool zip /*= true*/)
    : AbstractTokenNetworkJob(ApiType::Drive, 0, 0, driveDbId, 0),
      _dirId(dirId),
      _blacklist(blacklist),
      _zip(zip),
      _snapshotItemHandler(_logger) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_GET;
    _customTimeout = API_TIMEOUT + 15;

    if (_zip) {
        addRawHeader("Accept-Encoding", "gzip");
    }
}

bool CsvFullFileListWithCursorJob::getItem(SnapshotItem &item, bool &error, bool &ignore) {
    error = false;
    ignore = false;

    return _snapshotItemHandler.getItem(item, _ss, error, ignore);
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

    // Check that the stringstream is not empty (network issues)
    _ss.seekg(0, std::ios_base::end);
    int length = _ss.tellg();
    if (length == 0) {
        LOG_ERROR(_logger, "Reply " << jobId() << " received with empty content.");
        return false;
    }

    // Check that the stringstream ends by a LF (0x0A)
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
                   L"Reply " << jobId() << L" received - length=" << length << L" value=" << Utility::s2ws(_ss.str()).c_str());
    }

    return true;
}

}  // namespace KDC
