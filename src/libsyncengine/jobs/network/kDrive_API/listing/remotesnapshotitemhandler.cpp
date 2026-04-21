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

#include "remotesnapshotitemhandler.h"

#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"

namespace KDC {

static const std::string endOfFileDelimiter("#EOF");

RemoteSnapshotItemHandler::RemoteSnapshotItemHandler(const UserDbId userDbId, const DriveId driveId,
                                                     const log4cplus::Logger &logger) :
    _userDbId(userDbId),
    _driveId(driveId),
    _logger(logger) {}

void RemoteSnapshotItemHandler::logError(const std::wstring &methodName, const std::wstring &stdErrorType, const std::string &str,
                                         const std::exception &exc) {
    const std::wstring header = L"Error in SnapshotItem::" + methodName;
    const std::wstring strStr = L"str='" + CommonUtility::s2ws(str) + L"', ";
    const std::wstring excStr = L"exc='" + stdErrorType + L"', " + L"err=" + CommonUtility::s2ws(exc.what()) + L"'.";

    const std::wstring msg = header + strStr + excStr;

    LOGW_WARN(_logger, msg);
}

bool RemoteSnapshotItemHandler::updateRemoteSnapshotItem(const std::string &str, const CsvIndex index, RemoteSnapshotItem &item) {
    switch (index) {
        case CsvIndexId: {
            if (const auto exitInfo = item.setId(_userDbId, _driveId, str); !exitInfo) {
                LOG_WARN(_logger, "Error in SnapshotItem::setId");
                return false;
            }
            break;
        }
        case CsvIndexParentId: {
            if (const auto exitInfo = item.setParentId(_userDbId, _driveId, str); !exitInfo) {
                LOG_WARN(_logger, "Error in SnapshotItem::setParentId");
                return false;
            };
            break;
        }
        case CsvIndexName: {
            SyncName name = Str2SyncName(str);
#if defined(KD_WINDOWS)
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
                LOGW_WARN(_logger, L"Error in setSize, got a negative value - str='" << CommonUtility::s2ws(str) << L"'");
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
            // Ignore additional columns
            break;
        }
    }
    return true;
}

void RemoteSnapshotItemHandler::readRemoteSnapshotItemFields(RemoteSnapshotItem &item, const std::string &line, bool &error,
                                                             ParsingState &state) {
    for (const char c: line) {
        if (state.readingDoubleQuotedValue && state.prevCharDoubleQuotes) {
            if (c != ',' && c != '"') {
                // After a closing double quote, we must have a comma or another double quote. Otherwise, ignore the line.
                state.index = CsvIndexId; // Make sure that `state` is not equal to `CsvIndexEnd`.
                state.readingDoubleQuotedValue = false; // Exit the `readingDoubleQuotedValue` mode.
                LOG_WARN(_logger, "Item '" << line.c_str()
                                           << "' ignored because a closing double quote character must be followed by a comma or "
                                              "another double quote");

                return;
            }
        }

        if (c == ',' && (!state.readingDoubleQuotedValue || state.prevCharDoubleQuotes)) {
            state.readingDoubleQuotedValue = false;
            state.prevCharDoubleQuotes = false;
            if (!updateRemoteSnapshotItem(state.tmp, state.index, item)) {
                LOGW_WARN(_logger, L"Error in readSnapshotItemFields - line='" << CommonUtility::s2ws(line) << L"'.");
                error = true;
                return;
            }
            state.tmp.clear();
            incrementCsvIndex(state.index);
        } else if (c == '"') {
            if (state.index != CsvIndexName) {
                // Double quotes are only allowed within file and directory names.
                LOG_WARN(_logger, "Item '" << line << "' ignored because the '\"' character is only allowed in the name field");
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

bool RemoteSnapshotItemHandler::getItem(RemoteSnapshotItem &item, std::stringstream &ss, bool &error, bool &ignore, bool &eof) {
    error = false;
    ignore = false;

    std::string line;
    (void) std::getline(ss, line);
    if (line.empty()) {
        return false;
    }

    if (_ignoreFirstLine) {
        // The first line of the CSV full listing consists of the column names:
        // "id,parent_id,name,type,size,created_at,last_modified_at,can_write,is_link"
        _ignoreFirstLine = false;
        line.clear();
        (void) std::getline(ss, line);
        if (line.empty()) {
            return false;
        }
    }

    if (line == endOfFileDelimiter) {
        LOG_INFO(_logger, "End of file reached");
        eof = true;
        return false;
    }

    ParsingState state;
    while (state.readNextLine) {
        state.readNextLine = false;

        // Ignore the lines containing escaped double quotes
        if (line.find(R"(\")") != std::string::npos) {
            LOGW_WARN(_logger, L"Line containing an escaped double quotes, ignored it - line=" << CommonUtility::s2ws(line));
            ignore = true;
            return true;
        }

        readRemoteSnapshotItemFields(item, line, error, state);
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
            (void) std::getline(ss, line);
        }
    }

    if (state.index < CsvIndexEnd - 1) {
        LOG_WARN(_logger, "Invalid item");
        ignore = true;
        return true;
    }

    // Update last value
    if (!updateRemoteSnapshotItem(state.tmp, state.index, item)) {
        LOGW_WARN(_logger, L"Error in updateRemoteSnapshotItem - line=" << CommonUtility::s2ws(line));
        error = true;
        return true;
    }

    return true;
}

} // namespace KDC
