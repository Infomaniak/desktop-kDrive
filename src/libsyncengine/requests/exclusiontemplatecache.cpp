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

#include "exclusiontemplatecache.h"
#include "parameterscache.h"
#include "libparms/db/parmsdb.h"
#include "libcommonserver/log/log.h"
#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<ExclusionTemplateCache> ExclusionTemplateCache::_instance = nullptr;

std::shared_ptr<ExclusionTemplateCache> ExclusionTemplateCache::instance() {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<ExclusionTemplateCache>(new ExclusionTemplateCache());
        } catch (std::exception const &) {
            return nullptr;
        }
    }

    return _instance;
}

ExclusionTemplateCache::ExclusionTemplateCache() {
    // Load exclusion templates
    if (!ParmsDb::instance()->selectAllExclusionTemplates(true, _defExclusionTemplates)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllExclusionTemplates");
        throw std::runtime_error("Failed to create ExclusionTemplateCache instance!");
    }

    if (!ParmsDb::instance()->selectAllExclusionTemplates(false, _userExclusionTemplates)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectAllExclusionTemplates");
        throw std::runtime_error("Failed to create ExclusionTemplateCache instance!");
    }

    populateUndeletedExclusionTemplates();
}

void ExclusionTemplateCache::populateUndeletedExclusionTemplates() {
    _undeletedExclusionTemplates.clear();

    for (const auto &exclusionTemplate : _defExclusionTemplates) {
        if (!exclusionTemplate.deleted()) {
            _undeletedExclusionTemplates.push_back(exclusionTemplate);
        }
    }

    for (const auto &exclusionTemplate : _userExclusionTemplates) {
        if (!exclusionTemplate.deleted()) {
            _undeletedExclusionTemplates.push_back(exclusionTemplate);
        }
    }

    updateRegexPatterns();
}

void ExclusionTemplateCache::updateRegexPatterns() {
    const std::lock_guard<std::mutex> lock(_mutex);
    _regexPatterns.clear();

    for (const auto &exclPattern : exclusionTemplates()) {
        std::string templateTest = exclPattern.templ();
        escapeRegexSpecialChar(templateTest);

        // Replace all * by .*? to accept any characters. ? for lazy matching (matches as few characters as possible)
        std::string regexPattern;
        if (templateTest[0] == '*') {
            regexPattern += ".*?";
        } else {
            regexPattern += "^";  // Start of string
        }

        std::vector<std::string> splitStr = Utility::splitStr(templateTest, '*');
        for (auto it = splitStr.begin(); it != splitStr.end();) {
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
            regexPattern += "$";  // End of string
        }

        _regexPatterns.emplace_back(std::regex(regexPattern), exclPattern);
    }
}

void ExclusionTemplateCache::escapeRegexSpecialChar(std::string &in) {
    std::string out;
    static const char metacharacters[] = R"(\.^$-+()[]{}|?)";
    out.reserve(in.size());
    for (const auto ch : in) {
        if (std::strchr(metacharacters, ch)) {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    in = out;
}

ExitCode ExclusionTemplateCache::update(bool def, const std::vector<ExclusionTemplate> &exclusionTemplates) {
    if (def) {
        _defExclusionTemplates = exclusionTemplates;
    } else {
        _userExclusionTemplates = exclusionTemplates;
    }

    // Update exclusion templates
    if (!ParmsDb::instance()->updateAllExclusionTemplates(def, def ? _defExclusionTemplates : _userExclusionTemplates)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateAllExclusionTemplates");
        return ExitCodeDbError;
    }

    populateUndeletedExclusionTemplates();

    return ExitCodeOk;
}

bool ExclusionTemplateCache::checkIfIsExcluded(const SyncPath &basePath, const SyncPath &relativePath, bool &isWarning,
                                               bool &isExcluded, IoError &ioError) noexcept {
    isExcluded = false;
    ioError = IoErrorSuccess;

    if (!checkIfIsAnExcludedHiddenFile(basePath, relativePath, isExcluded, ioError)) {
        return false;
    }

    if (isExcluded) {
        return true;
    }

    isExcluded = isExcludedTemplate(relativePath, isWarning);

    return true;
}

bool ExclusionTemplateCache::checkIfIsAnExcludedHiddenFile(const SyncPath &basePath, const SyncPath &relativePath,
                                                           bool &isExcluded, IoError &ioError) noexcept {
    isExcluded = false;
    ioError = IoErrorSuccess;

    if (!basePath.empty() && !ParametersCache::instance()->parameters().syncHiddenFiles()) {
        // Call from local FS observer
        SyncPath absolutePath = basePath / relativePath;
        bool isHidden = false;

        const bool success = IoHelper::checkIfIsHiddenFile(absolutePath, isHidden, ioError);
        if (!success) {
            LOGW_WARN(Log::instance()->getLogger(),
                      L"Error in IoHelper::checkIfIsHiddenFile: " << Utility::formatIoError(absolutePath, ioError).c_str());
            return false;
        }

        if (isHidden) {
            if (ParametersCache::instance()->parameters().extendedLog()) {
                LOGW_INFO(Log::instance()->getLogger(),
                          L"Item \"" << Path2WStr(absolutePath).c_str() << L"\" rejected because it is hidden");
            }
            isExcluded = true;
        }
    }

    return true;
}

bool ExclusionTemplateCache::isExcludedTemplate(const SyncPath &relativePath, bool &isWarning) noexcept {
    const std::lock_guard<std::mutex> lock(_mutex);
    const std::string fileName = SyncName2Str(relativePath.filename().native());
    for (const auto &pattern : _regexPatterns) {
        const std::string &patternStr = pattern.second.templ();
        isWarning = pattern.second.warning();
        switch (pattern.second.complexity()) {
            case ExclusionTemplateComplexitySimplest: {
                if (fileName == patternStr) {
                    if (ParametersCache::instance()->parameters().extendedLog()) {
                        LOGW_INFO(Log::instance()->getLogger(),
                                  L"Item \"" << Path2WStr(relativePath).c_str() << L"\" rejected because of rule \""
                                             << Utility::s2ws(pattern.second.templ()).c_str() << L"\"");
                    }
                    return true;  // Filename match exactly the pattern
                }
                break;
            }
            case ExclusionTemplateComplexitySimple: {
                std::string tmpStr = patternStr;
                bool atBegining = tmpStr[0] == '*';
                bool atEnd = tmpStr[tmpStr.length() - 1] == '*';
                std::string::size_type n = tmpStr.find('*');
                while (n != std::string::npos) {
                    tmpStr.erase(n, 1);
                    n = tmpStr.find('*');
                }

                bool exclude = false;
                if (atBegining && atEnd) {
                    // Template can be anywhere
                    exclude = fileName.find(tmpStr) != std::string::npos;
                } else if (atBegining) {
                    // Must be at the end only
                    exclude = Utility::endsWith(fileName, tmpStr);
                } else {
                    // Must be at the beginning only
                    exclude = Utility::startsWith(fileName, tmpStr);
                }

                if (exclude) {
                    if (ParametersCache::instance()->parameters().extendedLog()) {
                        LOGW_INFO(Log::instance()->getLogger(),
                                  L"Item \"" << Path2WStr(relativePath).c_str() << L"\" rejected because of rule \""
                                             << Utility::s2ws(pattern.second.templ()).c_str() << L"\"");
                    }
                    return true;  // Filename contains the pattern
                }
                break;
            }
            case ExclusionTemplateComplexityComplex:
            default: {
                if (std::regex_match(fileName, pattern.first)) {
                    if (ParametersCache::instance()->parameters().extendedLog()) {
                        LOGW_INFO(Log::instance()->getLogger(),
                                  L"Item \"" << Path2WStr(relativePath).c_str() << L"\" rejected because of rule \""
                                             << Utility::s2ws(pattern.second.templ()).c_str() << L"\"");
                    }
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

}  // namespace KDC
