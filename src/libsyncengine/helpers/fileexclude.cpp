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

#include "fileexclude.h"
#include "requests/exclusiontemplatecache.h"
#include "libparms/db/parmsdb.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

std::shared_ptr<FileExclude> FileExclude::_instance = nullptr;

std::shared_ptr<FileExclude> FileExclude::instance() {
    if (_instance == nullptr) {
        _instance = std::shared_ptr<FileExclude>(new FileExclude());
    }
    return _instance;
}

bool FileExclude::isExcluded(const SyncPath &relativePath, bool syncHiddenFiles) {
    bool isHidden = false;
    IoError ioError = IoError::Success;

    if (!syncHiddenFiles && IoHelper::checkIfIsHiddenFile(relativePath, isHidden, ioError) && isHidden) {
        return true;
    }

    for (const auto &pattern: getRegexPatterns()) {
        if (std::regex_match(relativePath.filename().string(), pattern.first)) {
            // TODO : send the associated warning if necessary
            // LOG4CPLUS_DEBUG(Log::instance()->getLogger(), "Item \"" << relativePath.string().c_str() << "\" rejected because of
            // rule \"" << pattern.second.templ().c_str() << "\"");
            return true;
        }
    }

    // Check all parents
    if (relativePath.has_parent_path() && relativePath != "/") {
        return isExcluded(relativePath.parent_path(), syncHiddenFiles);
    }

    return false;
}

const std::vector<std::pair<std::regex, ExclusionTemplate>> &FileExclude::getRegexPatterns() {
    if (ParmsDb::instance()->exclusionTemplateChanged()) {
        _regexPatterns.clear();

        for (const auto &exclPattern: ExclusionTemplateCache::instance()->exclusionTemplates()) {
            std::string templateTest = exclPattern.templ();
            escapeRegexSpecialChar(templateTest);

            // Replace all * by .*? to accept any characters. ? for lazy matching (matches as few characters as possible)
            std::string regexPattern;
            if (templateTest[0] == '*') {
                regexPattern += ".*?";
            } else {
                regexPattern += "^"; // Start of string
            }

            std::vector<std::string> splitStr = Utility::splitStr(templateTest, '*');
            for (std::vector<std::string>::const_iterator it = splitStr.begin(); it != splitStr.end();) {
                if (it->empty()) {
                    it++;
                    continue;
                }
                regexPattern += *it;

                it++;
                if (it != splitStr.end()) {
                    regexPattern += ".*?";
                }
            }

            if (templateTest[templateTest.size() - 1] == '*') {
                regexPattern += ".*?";
            } else {
                regexPattern += "$"; // End of string
            }

            _regexPatterns.push_back(std::make_pair(std::regex(regexPattern), exclPattern));
        }
    }

    return _regexPatterns;
}

void FileExclude::escapeRegexSpecialChar(std::string &in) {
    std::string out;
    static const char metacharacters[] = R"(\.^$-+()[]{}|?)";
    out.reserve(in.size());
    for (const auto &ch: in) {
        if (std::strchr(metacharacters, ch)) out.push_back('\\');
        out.push_back(ch);
    }
    in = out;
}

} // namespace KDC
